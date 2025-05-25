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
    asm volatile("nop");  // 10
    asm volatile("nop");  // 11
    asm volatile("nop");  // 12
    asm volatile("nop");  // 13
    asm volatile("nop");  // 14
    asm volatile("nop");  // 15
    asm volatile("nop");  // 17
    asm volatile("nop");  // 18

    asm volatile("nop");  // 19
    asm volatile("nop");  // 20
    asm volatile("nop");  // 21
    asm volatile("nop");  // 22
    asm volatile("nop");  // 23
    asm volatile("nop");  // 24
    asm volatile("nop");  // 25
    asm volatile("nop");  // 26
    // asm volatile("nop");  // 27

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
    // -------
    // 9 Nops = 95
    // 10 Nops = 97
    // 11 Nops = 99
    // 12 Nops = 101
    // 13 Nops = 103
    // 15 Nops = 107
    // 17 Nops = 109
    // --------
    // 18 Nops = 143
    // 25 Nops = 157
    // --------
    // 26 Nops = 196

    // Becuase the .align dircetives and cycle tracking instructions
    // cause some overhead, we can't use our initial cycle measurements to
    // predict. But, observing the trend, it seems every 8 instructions the
    // cycle count jumps, suggested a prefetch buffer of 8 instructions.

    



}
