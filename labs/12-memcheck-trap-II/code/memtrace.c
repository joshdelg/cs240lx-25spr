#include "rpi.h"
#include "memtrace.h"

#include "watchpoint.h"

#include "mmu.h"
// 140e exception handling support
#include "full-except.h"
// 140e helpers for getting exception reason.
#include "armv6-except.h"
// 140e code for full context switching
// (caller,callee and cpsr).
#include "switchto.h"

#include "sbrk-trap.h"

// 1 = we expect a domain fault.
// 0 = we expect a or a watchpoint fault.
// used to catch some mistakes.
static int expect_domain_fault_p = 1;

// right now we only allow a single checker.  wrap this
// up for multiple checkers.
static memtrace_fn_t pre;
static memtrace_fn_t post;
static void *data;

static int quiet_p = 0;
void memtrace_yap_off(void) { quiet_p = 1; }
void memtrace_yap_on(void)  { quiet_p = 0; }

static uint32_t trap_access = 0;
static uint32_t no_trap_access = 0;

static const uint32_t DOM_client = 0b01;
static const uint32_t heap_dom = 2;

static int trap_is_on_p(void) {
    return domain_access_ctrl_get() == trap_access;
}
static void trap_on(void) {
    domain_access_ctrl_set(trap_access);

    uint32_t v = domain_access_ctrl_get();
    assert(v = trap_access);
}
static void trap_off(void) {
    domain_access_ctrl_set(no_trap_access);

    uint32_t v = domain_access_ctrl_get();
    assert(v = no_trap_access);
}

// turn memtracing on: wrapper with extra error checking.
void memtrace_trap_enable(void) {
    // need at least one handler!
    assert(pre || post);
    // if not true, didn't init
    assert(trap_access && no_trap_access);
    assert(!trap_is_on_p());

    // trace("Passed checks. Turning trapping on\n");
    trap_on();
}

// turn memtracing off: wrapper with extra error checking.
void memtrace_trap_disable(void) {
    // if not true, didn't init
    assert(trap_access && no_trap_access);
    assert(trap_is_on_p());
    trap_off();
}

// XXX: a good extension: change this so you look at the
// actual instruction and get the actual bytes.
static inline unsigned inst_nbytes(uint32_t inst) {
    return 4;
}

static void data_fault(regs_t *r) {
    // sanity check that we still at SUPER
    //   - should make it so we can run at user level.
    if(mode_get(r->regs[16]) != SUPER_MODE)
        panic("got a fault not at SUPER level?\n");

    // output("Data Fault\n");

    // after a domain fault: call <pre>.  
    // after a watchpoint fault: call <post>.
    uint32_t reason = data_abort_reason();
    if (reason == DOMAIN_SECTION_FAULT) {
        uint32_t fault_addr = data_abort_addr();
        uint32_t pc = r->regs[15];
        
        // Turn trap off in case fault handler accesses trapped memory
        trap_off();

        // trace("Domain Fault (Mem Trap) -- fault_addr=%x, pc=%x\n", fault_addr, pc);
        
        fault_ctx_t fault_ctx = fault_ctx_mk(r, fault_addr, inst_nbytes(fault_addr), data_fault_from_ld());
        if (pre) pre(data, &fault_ctx);

        // Turn watchpoint on last in case fault handler accesses trapped memory
        watchpt_on(fault_addr);        
    } else if (watchpt_fault_p()) {
        
        uint32_t fault_addr = watchpt_fault_addr();
        uint32_t pc = watchpt_fault_pc();
        
        // Turn watchpoint off first in case fault handler accesses trapped memory
        watchpt_off(fault_addr);

        // trace("Watchpoint -- fault_addr=%x, fault_pc=%x\n", fault_addr, pc);

        fault_ctx_t fault_ctx = fault_ctx_mk(r, fault_addr, inst_nbytes(fault_addr), watchpt_load_fault_p());
        fault_ctx.pc = pc; // Automatically gets set to regs[15] which is wrong

        if (post) post(data, &fault_ctx);

        // Turn trap on last in case fault handler accesses trapped memory
        trap_on();
    }

    // drain printk to avoid the "can tx" race in UART.
    while(!uart_can_put8())
        ;

    if(!quiet_p) output("Switching back to user mode\n");
    switchto(r);
}

// initialize memtrace system.
void memtrace_init(
    void *data_h,
    memtrace_fn_t pre_h,
    memtrace_fn_t post_h,
    unsigned trap_dom) {

    // setting up VM does not belong here, but we do it to keep things
    // simple for today's lab.
    assert(!mmu_is_enabled());
    sbrk_init();
    assert(mmu_is_enabled());

    pre = pre_h;
    post = post_h;
    if(!pre && !post)
        panic("must supply one handler: pre=%x, post=%x\n", pre,post);
    data = data_h;
    assert(trap_dom < 16);

    // Initialize domain register values
    uint32_t dom_bits = DOM_client << (1*2) | DOM_client << (2*2);

    // this only has the kernel domain: 
    // this will trap any heap acces.
    trap_access     = dom_bits;
    no_trap_access  = trap_access |  DOM_client << (trap_dom*2);

    // XXX: what's the right way to handle SS exceptions at the same time?
    full_except_install(0);
    full_except_set_data_abort(data_fault);
}
