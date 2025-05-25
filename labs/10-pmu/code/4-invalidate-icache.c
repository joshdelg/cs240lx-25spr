#include "rpi.h"
#include "rpi-pmu.h"
// #include "cache-support.h"

// Run a NOP to put it in cache, track cycles
// Run a NOP (should now be cached), track cycles + icache misses

// Run N nops, track cycles and icache misses

// Invalidate ICache

void invalidate_icache(void) {
    asm volatile("mcr p15, 0, %0, c7, c5, 0" :: "r" (0));
    prefetch_flush();
}

void measure_nops(const char* msg) {
    uint32_t cyc_s, cyc_e;
    uint32_t icache_miss_s, icache_miss_e;
    uint32_t inst_stall_s, inst_stall_e;

    pmu_stmt_measure(msg,
        icache_miss,
        inst_stall,
        {
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");

            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
        });
}

void notmain(void) {
    // cache_flush_all();
    for(int i = 0; i < 3; i++)
        measure_nops("Cache Off");
    output("---------------------------------------------------\n");
    
    caches_enable();

    for(int i = 0; i < 3; i++)
        measure_nops("Cache On");
    output("---------------------------------------------------\n");

    invalidate_icache();

    for(int i = 0; i < 3; i++) {
        invalidate_icache();
        measure_nops("Cache On (invalidated)");
    }

    // Expect third test has same cycles + cache misses as first
}