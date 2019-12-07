//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*
  
  This file describes a streaming prefetcher. Prefetches are issued after
  a spatial locality is detected, and a stream direction can be determined.

  Prefetches are issued into the L2 or LLC depending on L2 MSHR occupancy.

 */

#include <stdio.h>
#ifndef COMBINED_PREFETCHER
#include "../inc/prefetcher.h"
#include "combined.h"
#endif

#define STREAM_DETECTOR_COUNT 64
#define STREAM_WINDOW 16
#define STREAM_PREFETCH_DEGREE 2

typedef struct stream_detector
{
  // which 4 KB page this detector is monitoring
  unsigned long long int page;
  
  // + or - direction for the stream
  int direction;

  // this must reach 2 before prefetches can begin
  int confidence;

  // cache line index within the page where prefetches will be issued
  int pf_index;
} stream_detector_t;

stream_detector_t stream_detectors[STREAM_DETECTOR_COUNT];
int stream_replacement_index;

void stream_l2_prefetcher_initialize(int cpu_num)
{
  printf("Streaming Prefetcher\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);

  int i;
  for(i=0; i<STREAM_DETECTOR_COUNT; i++)
    {
      stream_detectors[i].page = 0;
      stream_detectors[i].direction = 0;
      stream_detectors[i].confidence = 0;
      stream_detectors[i].pf_index = -1;
    }

  stream_replacement_index = 0;
}

void stream_l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  // fprintf(stderr, "stream(%lld 0x%llx 0x%llx %d %d %d) ", get_current_cycle(0), addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));

  unsigned long long int cl_address = addr>>6;
  unsigned long long int page = cl_address>>6;
  int page_offset = cl_address&63;

  // check for a detector hit
  int detector_index = -1;

  int i;
  for(i=0; i<STREAM_DETECTOR_COUNT; i++)
    {
      if(stream_detectors[i].page == page)
	{
	  detector_index = i;
	  break;
	}
    }

  if(detector_index == -1)
    {
      // this is a new page that doesn't have a detector yet, so allocate one
      detector_index = stream_replacement_index;
      stream_replacement_index++;
      if(stream_replacement_index >= STREAM_DETECTOR_COUNT)
	{
	  stream_replacement_index = 0;
	}

      // reset the oldest page
      stream_detectors[detector_index].page = page;
      stream_detectors[detector_index].direction = 0;
      stream_detectors[detector_index].confidence = 0;
      stream_detectors[detector_index].pf_index = page_offset;
    }

  // train on the new access
  if(page_offset > stream_detectors[detector_index].pf_index)
    {
      // accesses outside the STREAM_WINDOW do not train the detector
      if((page_offset-stream_detectors[detector_index].pf_index) < STREAM_WINDOW)
	{
	  if(stream_detectors[detector_index].direction == -1)
	    {
	      // previously-set direction was wrong
	      stream_detectors[detector_index].confidence = 0;
	    }
	  else
	    {
	      stream_detectors[detector_index].confidence++;
	    }

	  // set the direction to +1
	  stream_detectors[detector_index].direction = 1;
	}
    }
  else if(page_offset < stream_detectors[detector_index].pf_index)
    {
      // accesses outside the STREAM_WINDOW do not train the detector
      if((stream_detectors[detector_index].pf_index-page_offset) < STREAM_WINDOW)
	{
          if(stream_detectors[detector_index].direction == 1)
            {
	      // previously-set direction was wrong
	      stream_detectors[detector_index].confidence = 0;
            }
          else
            {
	      stream_detectors[detector_index].confidence++;
            }

	  // set the direction to -1
          stream_detectors[detector_index].direction = -1;
        }
    }

  // prefetch if confidence is high enough
  if(stream_detectors[detector_index].confidence >= 2)
    {
      int i;
      for(i=0; i<STREAM_PREFETCH_DEGREE; i++)
	{
	  stream_detectors[detector_index].pf_index += stream_detectors[detector_index].direction;

	  if((stream_detectors[detector_index].pf_index < 0) || (stream_detectors[detector_index].pf_index > 63))
	    {
	      // we've gone off the edge of a 4 KB page
	      break;
	    }

	  // perform prefetches
	  unsigned long long int pf_address = (page<<12)+((stream_detectors[detector_index].pf_index)<<6);
	  
	  // check MSHR occupancy to decide whether to prefetch into the L2 or LLC
	  if(get_l2_mshr_occupancy(0) > 8)
	    {
	      // conservatively prefetch into the LLC, because MSHRs are scarce
	      fake_l2_prefetch_line(0, addr, pf_address, FILL_LLC, STREAM);
	    }
	  else
	    {
	      // MSHRs not too busy, so prefetch into L2
	      fake_l2_prefetch_line(0, addr, pf_address, FILL_L2, STREAM);
	    }
	}
    }
    // fprintf(stderr,"stream end;\n");
}