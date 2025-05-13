/**
 * @joshdelg
 * Approach: Domain fault traps before instruction runs, use it to set a watchpoint on address
 * Then, continue, call handler, and watchpoint will then run instruction
 */

#include "rpi.h"
#include "pinned-vm.h"
#include "full-except.h"
#include "armv6-except.h"
#include "switchto.h"
#include "memmap-default.h"
#include "watchpoint.h"
#include "rpi-rand.h"
#include "fast-hash32.h"

enum { 
    // pick some unused domain id.
    kern_dom = 1,
    heap_dom = 2,
    
    // pre-compute the domain register values
    // that we need.
    //
    //  - <DOM_client> = hardware checks the page
    //    permissions.
    //  - each domain is 2 bits so we have to multiply
    //    by 2.

    // this only has the kernel domain: 
    // this will trap any heap acces.
    trap_heap_access = DOM_client << (kern_dom*2),

    no_trap          = trap_heap_access 
                     |  DOM_client << (heap_dom*2)
};

typedef void (*memtrace_handler_t)(regs_t *r, uint32_t fault_addr, int load_p);

static memtrace_handler_t memtrace_handler = 0;

// start trapping heap accesses by switching the
// domain register.
static void trap_on(void) {
    domain_access_ctrl_set(trap_heap_access);

    // sanity check.  remove for speed.
    uint32_t v = domain_access_ctrl_get();
    assert(v = trap_heap_access);
}

// turn heap-trapping off.
static void trap_off(void) {
    domain_access_ctrl_set(no_trap);

    // sanity check.  remove for speed.
    uint32_t v = domain_access_ctrl_get();
    assert(v = no_trap);
}

// we need virtual memory for trapping.  so setup the
// simplest possible VM: 
//   identity mapping of only the 1mb sections used by 
//   our basic process (code, data, heap, stack and 
//   exception stack).  
// we pin these entries in the tlb so we don't even 
// need a page table.  
//
// we tag the heap with its own domain id (<heap_dom>), 
// and everything else with a different one <kern_dom>
//
// to keep things simple, we specialize this to what we
// need with our simple memtrap tests.
//
static int vm_map_everything(void) {
    // initialize the hardware MMU for pinned vm
    pin_mmu_init(no_trap);
    assert(!mmu_is_enabled());


    // compute the different mapping attributes.  
    // we only do simple uncached mappings today
    // (but shouldn't matter).

    // device memory: kernel domain, no user access, 
    // memory is strongly ordered, not shared.
    // we use 16mb section.
    pin_t dev  = pin_16mb(pin_mk_global(kern_dom, no_user, MEM_device));

    // kernel memory: same as device, but is only uncached.  
    pin_t kern = pin_mk_global(kern_dom, no_user, MEM_uncached);

    // heap.  different from kernel memory b/c:
    // 1. needs a different domain so will trap.
    // 2. user_access: since when we add single stepping 
    //    the code will run at user level.  (alternatively
    //    we could set <heap_dom> to manager permission)
    pin_t heap = pin_mk_global(heap_dom, user_access, MEM_uncached);

    // now identity map kernel memory.
    unsigned idx = 0;
    pin_mmu_sec(idx++, SEG_CODE, SEG_CODE, kern);

    // we could mess with the alignment to give the
    // heap more memory.
    pin_mmu_sec(idx++, SEG_HEAP, SEG_HEAP, heap);
    pin_mmu_sec(idx++, SEG_STACK, SEG_STACK, kern);
    pin_mmu_sec(idx++, SEG_INT_STACK, SEG_INT_STACK, kern);
    pin_mmu_sec(idx++, SEG_BCM_0, SEG_BCM_0, dev);

    // we aren't using user processes or anythings so we
    // just claim ASID=1 as our address space identifier.
    enum { ASID = 1 };
    pin_set_context(ASID);

    // turn the MMU on.
    assert(!mmu_is_enabled());
    mmu_enable();
    assert(mmu_is_enabled());
    // vm is now live!

    // return index in case if want to allocate more.
    return idx;
}

// simple data_fault handler to illustrate
// how to handle trapping memory operations.
static void data_fault(regs_t *r) {
    // b4-43 [140e pinned mem]
    uint32_t reason     = data_abort_reason();

    // b4-20 has the different reasons.
    if(reason == DOMAIN_SECTION_FAULT) {
        // @joshdelg Get the trapped VM addr
        // b4-44 [140e pinned mem]
        uint32_t fault_addr = data_abort_addr();
        uint32_t pc = r->regs[15];

        output("Domain Fault (Mem Trap) -- fault_addr=%x, pc=%x\n", fault_addr, pc);
        
        // @joshdelg Rather than emulate the instruction, we can just set a watchpoint
        watchpt_on(fault_addr);

        // Make sure trapping is off to avoid infinite loop
        trap_off();

        // Jump back to the next instruction
        switchto(r);
    } else if(watchpt_fault_p()) {
        // Run memcheck handler routine
        if(!memtrace_handler) panic("memtrace_handler is not set\n");

        // Get the watchpoint addr and pc
        uint32_t fault_addr = watchpt_fault_addr();
        uint32_t fault_pc = watchpt_fault_pc();

        output("Watchpoint -- fault_addr=%x, fault_pc=%x\n", fault_addr, fault_pc);

        memtrace_handler(r, fault_addr, watchpt_load_fault_p());
        
        // Turn off watchpoint
        watchpt_off(fault_addr);

        // Turn memory trapping back on
        trap_on();

        // Jump back to the next instruction
        switchto(r);
    } else {
        panic("unexpected fault: %b\n", reason);
    }
}

// we don't expect prefetch faults for this code.
static void prefetch_fault(regs_t *r) {
    panic("we got a prefetch abort fault at pc=%x\n", r->regs[15]);
}

static void my_memtrace_handler(regs_t *r, uint32_t fault_addr, int load_p) {
    output("my_memtrace_handler: fault_addr=%x, load_p=%d\n", fault_addr, load_p);
}

void notmain(void) { 
    // our kmalloc standard init.
    kmalloc_init_set_start((void*)SEG_HEAP, MB(1));

    // setup the full fault handlers [140e] that take in
    // the full register structure --- all 16 general
    // registers and the cpsr  --- that were live at the 
    // fault.
    full_except_install(0);
    full_except_set_data_abort(data_fault);
    full_except_set_prefetch(prefetch_fault);

    // map everything: when this returns vm is on!
    int idx = vm_map_everything();
    assert(mmu_is_enabled());

    // get the current domain.
    let x = domain_access_ctrl_get();
    output("%d total mappings, domain = %b\n", idx, x);

    // make sure trapping is off while we mess with the
    // heap.
    trap_off();

    uint32_t *v = kmalloc(sizeof *v);
    uint32_t *v_test2 = kmalloc(sizeof *v_test2);
    uint32_t *v_test3 = kmalloc((sizeof *v_test3) * 1024);

    // Make sure to 0 out array (should be already but just in case)
    memset(v_test3, 0, sizeof *v_test3 * 1024);

    // @joshdelg Provide memtrace handler
    memtrace_handler = my_memtrace_handler;

    trap_on();

    // Test 1: Just PUT32/GET32
    // do <N> trials where we read/write <v>
    // using GET32/PUT32 and validate the result.
    output("\n\nTest 1: Just PUT32/GET32");
    enum { N = 10 };
    for(int i = 0; i < N; i++) {
        output("about to do a PUT32!\n");

        // write a 32 bit value so we can
        // make sure no byte got messed up or
        // ignored.
        uint32_t expect = 0xfaf0faf0+i;
        put32(v, expect);
        output("about to do a GET!\n");
        uint32_t got = get32(v);

        // check that what we read equals what we
        // wrote.
        if(expect != got)
            panic("failed: got=%x, expect=%x\n", got, expect);
        else
            output("%d: success: got=%x, expect=%x\n", 
                                        i,got, expect);
    }
    output("SUCCESS!  passsed %d trials\n", N);

    // Test 2: Memory access without GET/PUT32
    output("\n\nTest 2: Memory access without GET/PUT32");

    for(int i = 0; i < N; i++) {
        // Generate a random heap address
        // uint32_t* rand_addr = (uint32_t*)(SEG_HEAP) + ((rpi_rand32() %(1024 * 1024)) >> 2);
        output("Should see trap on addr %p\n", v_test2);

        // Write to address
        uint32_t expect = 0xdeadbeef + i;
        *v_test2 = expect;

        // Read from address
        uint32_t got = *v_test2;

        // Check that what we read equals what we wrote
        if(expect != got)
            panic("failed: got=%x, expect=%x\n", got, expect);
        else
            output("%d: success: got=%x, expect=%x\n", i, got, expect);
    }

    // Test 3: Memory accesses to array
    output("\n\nTest 3: Memory accesses to array");

    uint32_t random_indices[N];

    for(int i = 0; i < N; i++) {
        // Generate a random index 0 -> 1023
        uint32_t rand_idx = rpi_rand32() % 1024;
        random_indices[i] = rand_idx;
        output("Should see trap on addr %p\n", v_test3 + rand_idx);
        
        // Write to address
        uint32_t expect = 0xdeadbeef + i;
        v_test3[rand_idx] = expect;
        
        // Read from address
        uint32_t got = v_test3[rand_idx];
        
        // Check that what we read equals what we wrote
        if(expect != got)
            panic("failed: got=%x, expect=%x\n", got, expect);
        else
            output("%d: success: got=%x, expect=%x\n", i, got, expect);
    }

    for(int i = 0; i < N; i++) {
        output("Random idx: %d\n", random_indices[i]);
    }

    trap_off();
    // Hash contents of array
    uint32_t hash_trap = fast_hash32(v_test3, sizeof *v_test3 * 1024);
    trap_on();

    // // Test 3b: Repeat with no traps and ensure is the same
    output("\n\nTest 3b: Repeat with no traps and ensure is the same");

    trap_off();
    
    // // Make sure to 0 out array again
    memset(v_test3, 0, sizeof *v_test3 * 1024);
    
    for(int i = 0; i < N; i++) {
        uint32_t rand_idx = random_indices[i];
        output("Should see trap on addr %p\n", v_test3 + rand_idx);
        
        // Write to address
        uint32_t expect = 0xdeadbeef + i;
        v_test3[rand_idx] = expect;
    }
    output("SUCCESS!  passsed %d trials\n", N);

    // // Hash contents of array
    uint32_t hash_no_trap = fast_hash32(v_test3, sizeof *v_test3 * 1024);

    // Check that hashes are the same
    if(hash_trap != hash_no_trap)
        panic("hashes are not the same: %x vs %x\n", hash_trap, hash_no_trap);
    else
        output("SUCCESS!  hashes are the same: %x\n", hash_trap);
}
