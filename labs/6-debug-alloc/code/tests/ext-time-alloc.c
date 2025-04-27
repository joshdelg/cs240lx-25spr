// simple use-after free: allocate, free, write one byte. 
// should get an error.
#include "rpi.h"
#include "ckalloc.h"
#include "cycle-count.h"

void notmain(void) {
    cycle_cnt_init();
    printk("Testing faster allocator\n");

    // printk("Timing Original Allocation\n");
    // TIME_CYC_PRINT("Allocate 128 byte block", ckalloc(128));

    // Random block sizes 1 bytes -> 1024 bytes
    // Generate random [0, 10] left shift by that value

    // Iterate 1000 times

    // 75% alloc, 25% free
    // alloc - pick a random block size, add the ptr to list
    // free - pick a random pointer from list, remove it
    size_t ptr_count = 0;

    uint32_t total_alloc_cycles = 0;
    uint32_t total_free_cycles = 0;

    uint32_t total_alloc_fast_cycles = 0;
    uint32_t total_free_fast_cycles = 0;

    static uint32_t alloc_sizes[1000];
    static uint32_t free_indices[1000];

    static uint32_t turns[1000]; // 1 alloc, -1 free, 0 stop

    uint32_t alloc_ptr = 0;
    uint32_t free_ptr = 0;
    uint32_t turn_ptr = 0;

    for(int i = 0; i < 1000; i++) {
        // trace("Round %d\n", i);
        unsigned do_alloc = ptr_count > 0 ? (rpi_rand32() % 4) <= 2 : 1; // Always allocate if no allocated
        // trace("Allocating: %d\n", do_alloc);

        if(do_alloc) {
            unsigned sz = 1 << (rpi_rand32() % 11);
            // trace("Allocating block of %d bytes\n", sz);

            uint32_t s = cycle_cnt_read();
            hdr_t *h = ckalloc(sz);
            uint32_t e = cycle_cnt_read();
            ptr_count++;

            alloc_sizes[alloc_ptr++] = sz;
            turns[turn_ptr++] = 1;

            // trace("Took %u cycles to allocate\n", e - s);
            // uint32_t s1 = cycle_cnt_read();
            // hdr_t *h2 = ckalloc_fast(sz, SRC_LOC_MK());
            // uint32_t e1 = cycle_cnt_read();

            total_alloc_cycles += e - s;
            // total_alloc_fast_cycles += e1 - s1;
            
        } else {
            uint32_t idx = rpi_rand32() % 1000;
            if(idx >= ptr_count) idx = ptr_count - 1;

            turns[turn_ptr++] = -1;
            free_indices[free_ptr++] = idx;

            hdr_t *h = ck_first_alloc();
            for(int i = 0; i < idx; i++) {
                h = ck_next_hdr(h);
            }

            // trace("Freeing block %u [%p] with size %d\n", h->block_id, ck_data_start(h), h->nbytes_alloc);
            uint32_t s = cycle_cnt_read();
            ckfree(ck_data_start(h));
            uint32_t e = cycle_cnt_read();

            // uint32_t s1 = cycle_cnt_read();
            // ckfree_fast(ck_data_start(h), SRC_LOC_MK());
            // uint32_t e1 = cycle_cnt_read();

            // trace("Took %u cycles to free\n", e - s);
            ptr_count--;

            total_free_cycles += e - s;
            // total_free_fast_cycles += e1 - s1;
        }
    }

    trace("Total Allocation cycles (x10^6): %u\n", total_alloc_cycles / 1000000);
    trace("Total Free cycles (x10^6): %u\n", total_free_cycles / 1000000);

    // Free all allocated blocks, use a curr and previous pointer
    hdr_t *curr = ck_first_alloc();
    hdr_t *prev = 0;
    while(curr) {
        prev = curr;
        curr = ck_next_hdr(curr);
        ckfree(ck_data_start(prev));
    }

    // Walk through alloc list
    for(hdr_t *h = ck_first_alloc(); h; h = ck_next_hdr(h)) {
        // trace("Allocated block %u with size %d\n", h->block_id, h->nbytes_alloc);
    }

    assert(ck_first_alloc() == 0);

    alloc_ptr = 0;
    free_ptr = 0;
    for(int i = 0; turns[i] != 0; i++) {
        if(turns[i] == 1) {
            uint32_t sz = alloc_sizes[alloc_ptr++];
            // trace("Allocating block of %d bytes\n", sz);

            uint32_t s1 = cycle_cnt_read();
            void *addr = ckalloc_fast(sz, SRC_LOC_MK());
            uint32_t e1 = cycle_cnt_read();

            hdr_t *h = ck_ptr_is_alloced(addr);

            // trace("Block ID: %u\n", h->block_id);

            total_alloc_fast_cycles += e1 - s1;

        } else if(turns[i] == -1) {
            uint32_t idx = free_indices[free_ptr++];
            
            hdr_t *h = ck_first_alloc();
            for(int i = 0; i < idx; i++) {
                h = ck_next_hdr(h);
            }
            
            // trace("Freeing block %u with size %d and pointer %p\n", h->block_id, h->nbytes_alloc, ck_data_start(h));

            uint32_t s1 = cycle_cnt_read();
            ckfree_fast(ck_data_start(h), SRC_LOC_MK());
            uint32_t e1 = cycle_cnt_read();

            total_free_fast_cycles += e1 - s1;
        }
    }

    trace("Total Allocation Fast cycles (x10^6): %u\n", total_alloc_fast_cycles / 1000000);
    trace("Total Free Fast cycles (x10^6): %u\n", total_free_fast_cycles / 1000000);

    // Generate a random number


    // char *p = ckalloc(4);
    // trace("alloc returned %u [%p]\n", ck_blk_id(p), p);
    // ckfree(p);
    // *p = 1;

    // if(!ck_heap_errors())
    //     panic("missed-error!\n");
    // else
    //     trace("SUCCESS found error\n");
}
