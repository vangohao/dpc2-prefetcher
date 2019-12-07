/*

  Based on 4 basic prefetchers, combined_prefetcher use a train phase to determine which prefetcher to use in the next learning phase. Each prefetcher is given a queue (depth=PREFETCH_CAPACITY) to store their prefetches to calculate the prefetch hit rate.

 */

#include <stdio.h>
#include "../inc/prefetcher.h"
#include "combined.h"
#include "ampm_lite.h"
#include "ip_stride.h"
#include "next_line.h"
#include "stream.h"

char PREFETCHER_NAMES[200] = "ampm\0\0\0\0nxtl\0\0\0\0ip\0\0\0\0\0\0stream\0\0";
#define ull unsigned long long int
#define MAX_PREFETCH_CAPACITY 16
// #define FIRST_LEARNING_PHASE_LENGTH 256
#define LEARNING_PHASE_LENGTH 2048
#define TURN_OFF_THRESHOLD 0

typedef struct
{
  ull base_addr;
  ull pf_addr;
  int fill_level;
}prefetch;

prefetch prefetcher_queues[4][MAX_PREFETCH_CAPACITY];   //the prefetched line addr
int prefetcher_flags[4];   // the head pointer of queue
int current_prefetcher;   // -1 = turn off prefetch
int best_prefetcher;
int phase;
int prefetcher_hits[4];
int highest_hits;

int fake_l2_prefetch_line(int cpu_num, unsigned long long int base_addr, unsigned long long int pf_addr, int fill_level, int prefetcher)
{
  // fprintf(stderr, "fake_l2_prefetch_line %llx %d\n", base_addr, prefetcher);
  if (current_prefetcher == prefetcher)
  {
    l2_prefetch_line(cpu_num, base_addr, pf_addr, fill_level);
  }
  prefetch tmp;
  tmp.base_addr = base_addr;
  tmp.pf_addr = pf_addr;
  tmp.fill_level = fill_level;
  prefetcher_queues[prefetcher][prefetcher_flags[prefetcher]] = tmp;
  prefetcher_flags[prefetcher] = (prefetcher_flags[prefetcher] + 1) % MAX_PREFETCH_CAPACITY;
  return 1;
}

void l2_prefetcher_initialize(int cpu_num)
{
  int i;
  printf("Combined Prefetching\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);
  ampm_l2_prefetcher_initialize(cpu_num);
  ip_l2_prefetcher_initialize(cpu_num);
  nextline_l2_prefetcher_initialize(cpu_num);
  stream_l2_prefetcher_initialize(cpu_num);
  phase = 0;
  best_prefetcher = -1;
  current_prefetcher = 0;
  for (i = 0; i < 4; i++)
  {
    prefetcher_flags[i] = 0;
    prefetcher_hits[i] = 0;
  }
  // fprintf(stderr, "ininital finish.\n");
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  // fprintf(stderr, "(0x%llx 0x%llx %d %d %d) \n", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));
  int i, j;
  for(i = 0; i < 4; i++)
  {
    int hit = 0;
    for(j = 0; j < 4; j++)
    {
      if ((prefetcher_queues[i][j].pf_addr >> 6) == (addr >> 6))
      {
        hit = 1;
      }
    }
    prefetcher_hits[i] += hit;
  }
  phase ++;
  ampm_l2_prefetcher_operate(cpu_num, addr, ip, cache_hit);
  ip_l2_prefetcher_operate(cpu_num, addr, ip, cache_hit);
  nextline_l2_prefetcher_operate(cpu_num, addr, ip, cache_hit);
  stream_l2_prefetcher_operate(cpu_num, addr, ip, cache_hit);
  if(phase == LEARNING_PHASE_LENGTH)
  {
    phase = 0;
    for (i = 0; i < 4; i++)
    {
      if (prefetcher_hits[i] > highest_hits)
      {
        best_prefetcher = i;
        highest_hits = prefetcher_hits[i];
      }
      prefetcher_hits[i] = 0;
    }
    if (highest_hits > TURN_OFF_THRESHOLD)
    {
      current_prefetcher = best_prefetcher;
      fprintf(stderr, "last learning period, best hits = %d, next prefetcher = %s\n", highest_hits, PREFETCHER_NAMES + current_prefetcher * 8);
    }
    else
    {
      current_prefetcher = -1;
      fprintf(stderr, "last learning period, best hits = %d, next prefetcher = 0\n", highest_hits);
    }
  }
  // fprintf(stderr, "end operate\n");
}

void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
  // uncomment this line to see the information available to you when there is a cache fill event
  //printf("0x%llx %d %d %d 0x%llx\n", addr, set, way, prefetch, evicted_addr);
}

void l2_prefetcher_heartbeat_stats(int cpu_num)
{
  printf("Prefetcher heartbeat stats\n");
}

void l2_prefetcher_warmup_stats(int cpu_num)
{
  printf("Prefetcher warmup complete stats\n\n");
}

void l2_prefetcher_final_stats(int cpu_num)
{
  printf("Prefetcher final stats\n");
}
