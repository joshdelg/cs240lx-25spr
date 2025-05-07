// a simple single step tracing example.
//
// lab: 
//   1. change this code so it is an instruction profiler 
//      (similar go gprof in 140e)
//   2. add PMU counters to make things more precise.
//      (you will have to change the <prefetch_abort_vector>
//      signature)
#include "rpi.h"
#include "ss-pixie.h"
#include "memmap.h"
#include "../../6-debug-alloc/code/ckalloc.h"
#include "../../6-debug-alloc/code/kr-malloc.h"

#include "breakpoint.h"
#include "cpsr-util.h"
// #include "rpi-math.h"

// counter of the number of instructions.
static volatile unsigned n_inst = 0;

static volatile uint32_t *inst_cts_p = 0;
static volatile uint32_t *cycle_cts_p = 0;

static volatile uint32_t prev_pc = 0;

// use a swi instruction to invoke system
//  see: <ss-pixie-asm.S>
int pixie_invoke_syscall(int syscall, ...);

// switch to SUPER mode and load <regs>
//  see: <ss-pixie-asm.S>
void pixie_switchto_super_asm(uint32_t regs[16]);

// switch to USER mode.
//  see: <ss-pixie-asm.S>
void pixie_switchto_user_asm(void);

// control output a bit: if non-zero print, otherwise don't.
#define pixout(args...) \
    do { if(pixie_verbose_p) output(args); } while(0)

static int pixie_verbose_p = 1;
void pixie_verbose(int verbose_p) {
    pixie_verbose_p = verbose_p;
}

// called on each single-step exception. see
// <ss-pixie-asm.S>
void prefetch_abort_vector(uint32_t pc) {
    if(!brkpt_fault_p())
        panic("have a non-breakpoint fault\n");

    n_inst++;
    pixout("%d: ss fault: pc=%x\n", n_inst, pc);

    // Increment count on table
    uint32_t pc_arr_off = (pc - (uint32_t)__code_start__) / sizeof(uint32_t);

    if(inst_cts_p)
        inst_cts_p[pc_arr_off]++;

    // Get end cycle counter
    uint32_t end_cycles;
    asm volatile("MRC p15, 0, %0, c13, c0, 4" : "=r"(end_cycles));

    // Get start cycle counter
    uint32_t start_cycles;
    asm volatile("MRC p15, 0, %0, c13, c0, 2" : "=r"(start_cycles));

    // Increment count on table
    // If n_inst == 1 -> finish time is not valid
    if(cycle_cts_p) {
        if(n_inst != 1 && prev_pc) {
            uint32_t prev_pc_arr_off = (prev_pc - (uint32_t)__code_start__) / sizeof(uint32_t);
            cycle_cts_p[prev_pc_arr_off] += (end_cycles - start_cycles);
        }
    }

    prev_pc = pc;

    // set a mismatch on the fault pc so that we can run it.
    brkpt_mismatch_set(pc);

    // if you don't print: you don't have to do this.
    // if you do print, there is a race condition if we 
    // are stepping through the UART code --- note: we could
    // just check the pc and the address range of
    // uart.o
    while(!uart_can_put8())
        ;
}

// called before dying.  we make the symbol weak so
// that if the test case has, will override.
// this lets test cases do thier own internal
// checking.
void WEAK(pixie_die_handler)(uint32_t regs[16]) {
    output("done: dying!\n");
}

// very limited system calls.  we only need these
// b/c SS has to run at USER level so can't switch
// back.
void pixie_syscall(int sysno, uint32_t *regs) {
    // check the saved spsr and make sure is USER level.
    if(mode_get(spsr_get()) != USER_MODE)
        panic("not at USER level?\n");

    switch(sysno) {
    case PIXIE_SYS_DIE:
        pixie_die_handler(regs);
        clean_reboot();
    case PIXIE_SYS_STOP:
        pixout("done: pc=%x\n", regs[15]);
        // change to SUPER
        pixie_switchto_super_asm(regs);
        not_reached();
    default:
        panic("invalid syscall: %d\n", sysno);
    }
    not_reached();
}

// defines <cp_asm_set> macro.
#include "asm-helpers.h"

// 3-121 --- set exception jump table location.
cp_asm_set(vector_base_asm, p15, 0, c12, c0, 0)


// start single step mismatching.
//  1. sets up exception vectors (idempotent).
//  2. enables single stepping (idempotent)
//  3. switches to USER mode.
//
// NOTE:
//   if you want to ignore instructions until 
//   return back outside of <pixie_start>
//   1. get the return address before switching
//      to USER mode
//         ignore_until_pc = __builtin_return_address(0);
//   2. in the single-step handler: ignore all PC values 
//      until <pc> equals <ignore_until_pc>
void pixie_start(void) {
    uint32_t N_INST = ((uint32_t)__code_end__ - (uint32_t) __code_start__) / sizeof(uint32_t);

    inst_cts_p = (uint32_t*) ckalloc(N_INST * sizeof(uint32_t));
    cycle_cts_p = (uint32_t*) ckalloc(N_INST * sizeof(uint32_t));

    memset((void*)inst_cts_p, 0, ((uint32_t)__code_end__ - (uint32_t) __code_start__) / sizeof(uint32_t));
    memset((void*)cycle_cts_p, 0, ((uint32_t)__code_end__ - (uint32_t) __code_start__) / sizeof(uint32_t));

    // vector of exception handlers: see <ss-pixie-asm.S>
    // check alignment and then set the vector base to it.
    extern uint32_t pixie_exception_vec[];
    uint32_t vec = (uint32_t)pixie_exception_vec;
    unsigned rem = vec % 32;
    if(rem != 0)
        panic("interrupt vec not aligned to 32 bytes!\n", rem);
    vector_base_asm_set(vec);


    // enable mismatching.  note: we are currently
    // at privileged mode so won't start til we 
    // switch to to user mode.
    //
    // this routine is from 140e.  will: 
    //  1. enable the debug co-processor cp14 
    //  2. set a mismatch on address <0>.  once we are 
    //    at user-level, this will cause any pc != 0 (all 
    //    of them) to immediately mismatch
    brkpt_mismatch_start();

    // switch from SUPER to USER mode: not just a matter of
    // switching the cpsr since the sp,lr are different.

    // 1. sanity check that we are at SUPER_MODE
    if(mode_get(cpsr_get()) != SUPER_MODE)
        panic("not at SUPER level?\n");

    // 2. switch to USER.  will immediately
    // immediately start mismatching when
    // the mode switch occurs in routine. 
    pixie_switchto_user_asm();

    // 3. sanity check that at USER. 
    //
    // NOTE: this check adds extra instructions to the 
    // trace if you don't filter them (see note above)
    if(mode_get(cpsr_get()) != USER_MODE)
        panic("not at user level?\n");
}

// turn off single step matching. 
//
// we need to do a system call b/c we are running 
// at user level and thus can't write to the debug 
// co-processor
//
// alernative: catch the undefined instruction and 
// do it.
unsigned pixie_stop(void) {
    pixie_invoke_syscall(PIXIE_SYS_STOP);
    pixout("pixie: stopped!\n");

    // Divide cycle counts by number of instructions
    uint32_t N_INST = ((uint32_t)__code_end__ - (uint32_t) __code_start__) / sizeof(uint32_t);
    for(int i = 0; i < N_INST; i++) {
        if(cycle_cts_p[i] != 0)
            cycle_cts_p[i] /= inst_cts_p[i];
    }

    return n_inst;
}

void pixie_dump(unsigned N) {
    pixie_verbose(1);

    // Selection sort
    // trace("Start: %x End: %x\n", (uint32_t)__code_start__, (uint32_t)__code_end__);
    volatile uint32_t N_INST = ((uint32_t)__code_end__ - (uint32_t)__code_start__) / sizeof(uint32_t);
    volatile uint32_t pc_addrs[N_INST];
    for(int i = 0; i < N_INST; i++) {
        // pc_addrs[i] = ((uint32_t) __code_start__) + i * sizeof(uint32_t);
        pc_addrs[i] = 0x8000 + i * sizeof(uint32_t);
    }

    for(int i = 0; i < N_INST; i++) {
        int minIndex = i;
        for(int j = i; j < N_INST; j++) {
            if(inst_cts_p[j] < inst_cts_p[minIndex])
                minIndex = j;
        }

        uint32_t tmp = inst_cts_p[i];
        inst_cts_p[i] = inst_cts_p[minIndex];
        inst_cts_p[minIndex] = tmp;

        // Also swap in pc_addrs
        tmp = pc_addrs[i];
        pc_addrs[i] = pc_addrs[minIndex];
        pc_addrs[minIndex] = tmp;

        // Also swap cycle counts
        tmp = cycle_cts_p[i];
        cycle_cts_p[i] = cycle_cts_p[minIndex];
        cycle_cts_p[minIndex] = tmp;
    }

    pixout("Top %d Instruction counts:\n", N);
    for(int i = N_INST - 1; i >= N_INST - N; i--) {
        if(inst_cts_p[i] == 0)
            break;
        
        pixout("%x: %u (%u cycles per inst.)\n", pc_addrs[i], inst_cts_p[i], cycle_cts_p[i]);
    }

    // pixout("Cycle Counts:\n");
    // for(int i = 0; i < N_INST; i++) {        
    //     if(cycle_cts_p[i] != 0)
    //         pixout("%x: %u (%u cycles)\n", pc_addrs[i], cycle_cts_p[i]);
    // }
}
