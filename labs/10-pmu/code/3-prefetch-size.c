// simple PMU test: measure cycles, instructions and stalls
// using the raw interface.
#include "rpi.h"
#include "rpi-pmu.h"

__attribute__((noinline)) 
void measure_nops(const char *msg, int n) {
    uint32_t cyc_s, cyc_e;
    uint32_t inst0_s, inst0_e;
    uint32_t stall1_s, stall1_e;

    // enable the two events.
    pmu_enable(0, PMU_inst_cnt);
    pmu_enable(1, PMU_inst_stall);

    asm volatile(".align 4");

    cyc_s       = pmu_cycle_get();      // always on cycle counter
    inst0_s     = pmu_event_get(0);     // instruction count
    stall1_s    = pmu_event_get(1);     // stalls

    asm volatile("nop");  // 1
    asm volatile("nop");  // 2
    asm volatile("nop");  // 3
    asm volatile("nop");  // 4
    asm volatile("nop");  // 5
    asm volatile("nop");  // 6
    asm volatile("nop");  // 7
    asm volatile("nop");  // 8
    asm volatile("nop");  // 9

    cyc_e       = pmu_cycle_get();      // always on cycle counter
    inst0_e     = pmu_event_get(0);     // instruction count
    stall1_e    = pmu_event_get(1);     // stalls

    output("%d:%s: total cyc=%d, tot inst=%d, tot stalls=%d\n",
        n,
        msg,
        cyc_e - cyc_s,
        inst0_e - inst0_s,
        stall1_e - stall1_s);
}

void notmain(void) {
    caches_disable();
    flush_caches();

    for(int i = 0; i < 10; i++)
        measure_nops("no cache",i);

    // Observations:
    // 5 Nops - 53
    // 6 Nops - 55
    // 7 Nops = 57
    // 8 Nops = 59
    // 9 Nops = 95

    



}
