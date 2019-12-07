#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "../inc/prefetcher.h"

// pagesize : 4KB (2 ^ 12) , Line 64B (2 ^ 6)

#define bool int
#define true 1
#define false 0

#define ull unsigned long long
#define offset_list_count 62
#define BAD_SCORE 1
#define SCORE_MAX 31
#define ROUND_MAX 100
#define MSHR_THRESHOLD 8
int rr[256];
int offset_list[offset_list_count];
int scores[offset_list_count];
bool prefetch_bits[L2_SET_COUNT][L2_ASSOCIATIVITY];
int prefetch_offset;
int bestoffset;
int maxscore;
int phase; //learning
int ol_round;

// 备注: 
// delayed queue 暂未实现

bool samepage(ull addr0, ull addr1)
{
    return ((addr0 >> 6) == (addr1 >> 6));
}

int offsets(int i)
{
    return i + 1;
}

void rr_init()
{
    memset(rr, 0, sizeof(rr));
}

void rr_insert(ull lineaddr)
{
    rr[((lineaddr ^ (lineaddr >> 8)) & ((1 << 8) - 1))] = ((lineaddr >> 8) & ((1 << 12) - 1));
}

bool rr_hit(ull lineaddr)
{
    return rr[(lineaddr ^ (lineaddr >> 8)) & ((1 << 8) - 1)] == ((lineaddr >> 8) & ((1 << 12) - 1));
}

void ol_init()
{
    int i;
    for (i = 0; i < offset_list_count; i++)
    {
        offset_list[i] = i + 1;
        scores[i] = 0;
    }
    maxscore = 0;
}

void learning_phase_init()
{
    memset(scores, 0, sizeof(scores));
    phase = 0;
}

void l2_prefetcher_initialize(int cpu_num)
{
    printf("No Prefetching\n");
    // you can inspect these knob values from your code to see which configuration you're runnig in
    printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);
    rr_init();
    ol_init();
    learning_phase_init();
    memset(prefetch_bits, 0, sizeof(prefetch_bits));
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
    // uncomment this line to see all the information available to make prefetch decisions
    //printf("(0x%llx 0x%llx %d %d %d) ", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));
    ull lineaddr = addr >> 6;

    int s = l2_get_set(addr);
    int w = l2_get_way(0, addr, s);
    bool l2_hit = (w >= 0);
    bool prefetched = false;

    if (l2_hit)
    {
        prefetched = prefetch_bits[s][w];
        prefetch_bits[s][w] = false;
    }
    else
    {
        // pt_llc_access();
    }

    // dq_pop();

    bool prefetch_issued = false;

    if (!l2_hit || prefetched)
    {
        ull testlineaddr = lineaddr - offsets(phase);
        if (samepage(lineaddr, testlineaddr) && rr_hit(testlineaddr))
        {
            scores[phase]++;
            if (scores[phase] >= maxscore)
            {
                maxscore = scores[phase];
                bestoffset = offsets(phase);
            }
        }
        phase = (phase + 1) % offset_list_count;
        if (phase == 0)
        {
            ol_round ++;
            if ((maxscore == SCORE_MAX) || (ol_round == ROUND_MAX))
            {
                prefetch_offset = bestoffset;
                if (maxscore <= BAD_SCORE)
                {
                    prefetch_offset = 0;
                }
                ol_init();
            }
        }
        
        // issue a prefetch
        if (prefetch_offset && samepage(lineaddr, lineaddr + prefetch_offset))
        {
            if (get_l2_mshr_occupancy(0) < MSHR_THRESHOLD)
            {
                l2_prefetch_line(0,lineaddr << 6, (lineaddr + prefetch_offset) << 6, FILL_L2);
            }
            else
            {
                l2_prefetch_line(0,lineaddr << 6, (lineaddr + prefetch_offset) << 6, FILL_LLC);
            }
        }
    }
}

void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
    // uncomment this line to see the information available to you when there is a cache fill event
    //printf("0x%llx %d %d %d 0x%llx\n", addr, set, way, prefetch, evicted_addr);

    ull lineaddr = addr >> 6;
    int s = l2_get_set(addr);
    int w = l2_get_way(0, addr, s);
    prefetch_bits[s][w] = prefetch;

    ull baselineaddr;
    if (prefetch || (prefetch_offset == 0))
    {
        baselineaddr = lineaddr - prefetch_offset;
        if (samepage(lineaddr, baselineaddr))
        {
            rr_insert(baselineaddr);
        }
    }
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
