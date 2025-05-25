#include "rpi.h"
#include "rpi-pmu.h"

void branch_always() {
    volatile int x = 1;
    volatile int branched = 1;

    pmu_stmt_measure("Branch Always",
        branch_cnt,
        branch_miss,
        {
            asm volatile("cmp %1, #0\n"
                         "bne 1f\n"
                         "mov %0, #0\n"
                         "1:\n"
                         "nop\n"
                         : "+r" (branched)
                         : "r" (x)
                         :
            );
        }
    );

    // Cool note! Leaving this output in caused branch predictor
    // to always fail -- I guess there's too many branches in there lol
    // output("Branched?: %d\n", branched);
}

void branch_never(void) {
    volatile int x = 0;
    volatile int branched = 1;

    pmu_stmt_measure("Branch Never",
        branch_cnt,
        branch_miss,
        {
            asm volatile("cmp %1, #0\n"
                         "bne 1f\n"
                         "mov %0, #0\n"
                         "1:\n"
                         "nop\n"
                         : "+r" (branched)
                         : "r" (x)
                         :
            );
        }
    );

    // output("Branched?: %d\n", branched);
}

void branch_conditional(int should_branch) {
    volatile int branched = 1;

    pmu_stmt_measure("Branch Conditional",
        branch_cnt,
        branch_miss,
        {
            asm volatile("cmp %1, #0\n"
                         "bne 1f\n"
                         "mov %0, #0\n"
                         "1:\n"
                         "nop\n"
                         : "+r" (branched)
                         : "r" (should_branch)
                         :
            );
        }
    );
    
}

void notmain(void) {
    caches_enable();
    output("---------------------------------------------------\n");
    branch_always();
    branch_always();
    branch_always();

    // Fails, Succeeds, Succeeds
    cache_flush_all();
    output("---------------------------------------------------\n");
    branch_never();
    branch_never();
    branch_never();

    // Succeeds, Succeeds, Succeeds -> Starts at 0
    cache_flush_all();
    output("---------------------------------------------------\n");
    branch_conditional(1); // Fails
    branch_conditional(0); // Fails
    branch_conditional(0); // Succeeds

    cache_flush_all();
    output("---------------------------------------------------\n");
    branch_conditional(1); // Fails
    branch_conditional(1); // Succeeds -> 1
    branch_conditional(0); // Fails -> 2
    branch_conditional(1); // Succeeds -> 3

    // Classic 2-bit branch predictor starting with 01 first branch not taken, 10 if first branch taken
    // Manual 5-5 confirms this!

    
    
}