#define AMPM 0
#define NEXTLINE 1
#define IP 2
#define STREAM 3

int fake_l2_prefetch_line(int cpu_num, unsigned long long int base_addr, unsigned long long int pf_addr, int fill_level, int prefetcher);