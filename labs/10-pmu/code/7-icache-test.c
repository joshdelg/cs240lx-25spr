#include "rpi.h"
#include "rpi-pmu.h"
#include "bit-support.h"

void less_nops(void);
void many_nops(void);

uint32_t get_cache_details(void) {
    uint32_t val;
    asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(val));
    return val;
}

void test_less_nops(void) {
    pmu_stmt_measure("Less Nops", icache_miss, inst_cnt, {
        less_nops();
    });
}

void test_many_nops(void) {
    pmu_stmt_measure("Many Nops", icache_miss, inst_cnt, {
        many_nops();
    });
}

void notmain(void) {
    uint32_t val = get_cache_details();
    
    // icache size [9:6]
    uint32_t icache_size = bits_get(val, 6, 9);
    assert(icache_size == 0b0101);

    output("icache size: %d (16 KB)\n", icache_size);

    uint32_t icache_assoc = bits_get(val, 3, 5);
    assert(icache_assoc == 0b0010);
    
    output("icache assoc: %d (4-way)\n", icache_assoc);

    uint32_t icache_line_size = bits_get(val, 0, 1);
    assert(icache_line_size == 0b10);

    output("icache line size: %d (32 bytes)\n", icache_line_size);

    output("---------------------------------------------------\n");

    caches_enable();

    // cache_flush_all();

    test_less_nops();

    // cache_flush_all();

    test_less_nops();

    // Except second run to have no cache misses and run much faster

    cache_flush_all();

    test_many_nops();

    // cache_flush_all();

    test_many_nops();

    // Expect second run to have same amount of misses and run roughly the same speed
}