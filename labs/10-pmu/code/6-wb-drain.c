#include "rpi.h"
#include "rpi-pmu.h"
#include "rpi-rand.h"

void test_dsb_incr(void) {
    pmu_stmt_measure("Dsb Incr", wb_drain, inst_cnt, {
        dsb();
        dsb();
        dsb();
        dsb();
        dsb();
        
        dsb();
        dsb();
        dsb();
        dsb();
        dsb();
        
    });
}

void test_dmb_incr(void) {
    pmu_stmt_measure("Dmb Incr", wb_drain, inst_cnt, {
        dmb();
        dmb();
        dmb();
        dmb();
        dmb();
        
        dmb();
        dmb();
        dmb();
        dmb();
        dmb();
    });
}

void test_random_mix_ptrs(void) {
    // Generate 10 random 0 or 1
    int random_bits[10];
    void (*functions[10])(void);

    for (int i = 0; i < 10; i++) {
        random_bits[i] = rpi_rand16() % 2;
        functions[i] = random_bits[i] == 0 ? dsb : dmb;
    }

    pmu_stmt_measure("Random Mix", wb_drain, inst_cnt, {
        for (int i = 0; i < 10; i++) {
            functions[i]();
        }
    });
    
}

void test_random_mix_if(void) {
    pmu_stmt_measure("Random Mix If", wb_drain, inst_cnt, {
        for(int i = 0; i < 10; i++) {
            if (rpi_rand16() % 2 == 0) {
                dsb();
            } else {
                dmb();
            }
        }
    });
}

void test_fixed_mix(void) {
    pmu_stmt_measure("Fixed Mix", wb_drain, inst_cnt, {
        for(int i = 0; i < 10; i++) {
            if(i % 2 == 0) {
                dsb();
            } else {
                dmb();
            }
        }
    });
}

void notmain(void) {
    // caches_enable();
    caches_disable();
    output("---------------------------------------------------\n");

    test_dsb_incr();
    test_dmb_incr();
    test_random_mix_ptrs(); // Probably higher because of speculative execution
    test_random_mix_if();
    test_fixed_mix();
}