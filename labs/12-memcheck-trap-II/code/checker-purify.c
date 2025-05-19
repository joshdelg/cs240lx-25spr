// engler: cs240lx: a sort-of purify checker: gives an error
// message if a load/store to a heap addres is:
//   - not within a legal block.
//   - to freed memory.
//
// uses the checking allocator (ckalloc).  for the moment
// just reboot on an error.
// 
// limits:
//   - does not check that its within the *correct*
//     legal block (can do this w/ replay).
//   - does not track anything about global or stack memory.
#include "memtrace.h"
#include "ckalloc.h"
#include "purify.h"
#include "sbrk-trap.h"
#include "memmap-default.h"

static int purify_quiet_p = 0;
void purify_yap_off(void) {
    purify_quiet_p = 1;
    memtrace_yap_off();
}
void purify_yap_on(void) {
    purify_quiet_p = 0;
    memtrace_yap_on();
}

// turn off trapping when we allocate.  Q: what if we 
// don't do this?
void *purify_alloc_raw(unsigned n, src_loc_t l) {
    memtrace_trap_disable();

        // if shadow memory: mark [p,p+n) as allocated
        unsigned *p = (ckalloc)(n, l);

    memtrace_trap_enable();
    return p;
}

// turn off trapping when we free.  Q: what if we 
// don't do this?
void purify_free_raw(void *p, src_loc_t l) {
    memtrace_trap_disable();

        // if shadow memory: mark [p,p+n) as free
        (ckfree)(p, l);

    memtrace_trap_enable();
}

static int handler(void *data, fault_ctx_t *f) {
    hdr_t *blk = ck_ptr_is_alloced((uint32_t*) f->addr);

    if (!ck_ptr_is_alloced((uint32_t*) f->addr)) {
        output("Error: Accessing unallocated memory at address %x\n", f->addr);

        // Find exact error
        hdr_t *blk = ck_get_containing_blk((uint32_t*) f->addr);
        output("Illegal access contained in block: %x with status %s\n", blk, blk->state == ALLOCED ? "ALLOCED" : "FREED");

        int illegal_offset = ck_illegal_offset(blk, (uint32_t*) f->addr);

        if (illegal_offset > 0) {
            output("Attempted to access memory %d bytes after block end\n", illegal_offset);
        } else if (illegal_offset < 0) {
            output("Attempted to access memory %d bytes before block start\n", illegal_offset);
        } else {
            output("Attempted to access memory within block of status %s\n", blk->state == ALLOCED ? "ALLOCED" : "FREED");
        }

        // Reboot on error
        clean_reboot();
    }

    return MEMTRACE_OK;
}

void purify_init(void) {
    memtrace_init(0, handler, 0, dom_trap);
    memtrace_trap_enable();
    memtrace_yap_off();
}
