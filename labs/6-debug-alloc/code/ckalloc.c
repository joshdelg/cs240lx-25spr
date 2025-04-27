// implement a simple ckalloc/free that adds ckhdr_t to the 
// allocation.
#include "rpi.h"
#include "ckalloc.h"
#include "kr-malloc.h"

unsigned ck_verbose_p = 0;

// should hold all allocated blocks
static hdr_t *alloc_list; 
static hdr_t *free_list;

// Holds start of free blocks size 2^i
static hdr_t *fixed_lists[11];

// returns pointer to the first allocated header block.
hdr_t *ck_first_alloc(void) {
    return alloc_list;
}

// return header associated with <ptr> if one exists.
hdr_t *ck_ptr_is_alloced(void *ptr) {
    for(hdr_t *h = ck_first_alloc(); h; h = ck_next_hdr(h)) {
        if(ck_ptr_in_block(h,ptr)) {
            return h;
        }
    }
    return 0;
}


/***********************************************************************
 * implement the rest
 */

// is <ptr> inside <h>'s data block?
unsigned ck_ptr_in_block(hdr_t *h, void *ptr) {
    if(h->state != ALLOCED) {
        panic("should only have allocated blocks: ID=%d, state=%d\n", h->block_id, h->state);
        return 0;
    }

    // use ck_data_start/_end 
    // todo("check that <ptr> is in data for <h>\n");
    return ck_data_start(h) <= ptr && ptr < ck_data_end(h);
}


static void list_remove(hdr_t **l, hdr_t *h) {
    assert(l);
    hdr_t *prev = *l;
 
    if(prev == h) {
        *l = h->next;
        return;
    }

    hdr_t *p;
    while((p = ck_next_hdr(prev))) {
        if(p == h) {
            prev->next = p->next;
            return;
        }
        prev = p;
    }
    panic("did not find %p in list\n", h);
}

int mem_check(hdr_t *h) {
    // Check redzones
    for(int i = 0; i < REDZONE_NBYTES; i++) {
        if(ck_get_rz1(h)[i] != REDZONE_VAL) {
            ck_error(h, "%s block %u [%p] corrupted at offset %d\n",
                h->state == FREED ? "Freed" : "Allocated",
                    h->block_id, ck_data_start(h), -REDZONE_NBYTES + i);

            return 1; // Error
        }
    }

    for(int i = 0; i < REDZONE_NBYTES; i++) {
        if(ck_get_rz2(h)[i] != REDZONE_VAL) {
            ck_error(h, "%s block %u [%p] corrupted at offset %d\n",
                h->state == FREED ? "Freed" : "Allocated",
                    h->block_id, ck_data_start(h), h->nbytes_alloc + i);

            return 1; // Error
        }
    }

    // If freed, check data
    if(h->state == FREED) {
        for(int i = 0; i < h->nbytes_alloc; i++) {
            if(((uint8_t*)(ck_data_start(h)))[i] != REDZONE_VAL) {
                ck_error(h, "%s block %u [%p] corrupted at offset %d\n",
                    h->state == FREED ? "Freed" : "Allocated",
                        h->block_id, ck_data_start(h), i);
                return 2;
            }
        }
    }

    return 0;
}

int check_list(unsigned *nblks, hdr_t *list, int freed) {
    // List traversal
    unsigned nerrors = 0;
    for(hdr_t *h = list; h; h = ck_next_hdr(h)) {
        if((h->state != FREED) == freed)
            ck_error(h, "block %u [%p] has wrong state: %s\n", h->block_id, ck_data_start(h), h->state == FREED ? "freed" : "allocated");

        int err_type = mem_check(h);

        if(err_type == 1) {
            nerrors++;
        } else if(err_type == 2) {
            nerrors++;
            trace("\tWrote block after free!\n");
        }

        (*nblks)++;
    }
    return nerrors;
}

// integrity check the allocated / freed blocks in the heap
int ck_heap_errors(void) {
    trace("going to check heap\n");

    unsigned nblks = 0;

    unsigned nerrors = check_list(&nblks, alloc_list,0)
                     + check_list(&nblks, free_list, 1);

    if(nerrors)
        trace("checked %d blocks, detected %d errors\n", nblks, nerrors);
    else
        trace("SUCCESS: checked %d blocks, detected no errors\n", nblks);
    return nerrors;
}

void (ckfree_fast)(void *addr, src_loc_t l) {
    hdr_t *h = ck_ptr_is_alloced(addr);

    // Add to block list, then just free
    int log_nbytes = 0;
    for(int i = 0; i < 32; i++) {
        if(h->nbytes_alloc & (1 << i))
            log_nbytes = i;
    }

    if(!h)
        loc_panic(l, "freeing bogus pointer: %p\n", addr);

    // allocated block starts right after the header.

    void *blk_start = ck_data_start(h);
    if(blk_start != addr)
        loc_panic(l, "not freeing using start pointer: have %p, need %p\n",
            addr, blk_start);

    if(h->state != ALLOCED)
        loc_panic(l, "freeing unallocated memory: state=%d\n", h->state);
    if(ck_verbose_p)
        loc_debug(l, "freeing %p\n", addr);

    assert(ck_ptr_is_alloced(addr));

    // @joshdelg Check redzone integrity before freeing
    if(mem_check(h)) {
        ck_error(h, "corrupted block %u \n", h->block_id);
        clean_reboot();
    }

    h->state = FREED;

    // @joshdelg Set data portion to redzone value
    memset(ck_data_start(h), REDZONE_VAL, h->nbytes_alloc);

    // just remove from the allocated list.
    list_remove(&alloc_list, h);
    assert(!ck_ptr_is_alloced(addr));

    // kr_free(h);

    // Add to indexed free list
    h->next = fixed_lists[log_nbytes];
    fixed_lists[log_nbytes] = h;
}

// free a block allocated with <ckalloc>
void (ckfree)(void *addr, src_loc_t l) {
    hdr_t *h = ck_ptr_is_alloced(addr);
    if(!h)
        loc_panic(l, "freeing bogus pointer: %p\n", addr);

    // allocated block starts right after the header.

    void *blk_start = ck_data_start(h);
    if(blk_start != addr)
        loc_panic(l, "not freeing using start pointer: have %p, need %p\n",
            addr, blk_start);

    if(h->state != ALLOCED)
        loc_panic(l, "freeing unallocated memory: state=%d\n", h->state);
    if(ck_verbose_p)
        loc_debug(l, "freeing %p\n", addr);

    assert(ck_ptr_is_alloced(addr));

    // @joshdelg Check redzone integrity before freeing
    if(mem_check(h)) {
        ck_error(h, "corrupted block %u \n", h->block_id);
        clean_reboot();
    }

    h->state = FREED;

    // @joshdelg Set data portion to redzone value
    memset(ck_data_start(h), REDZONE_VAL, h->nbytes_alloc);

    // just remove from the allocated list.
    list_remove(&alloc_list, h);
    assert(!ck_ptr_is_alloced(addr));

    // kr_free(h);

    // Add to free list
    h->next = 0;
    if(!free_list) {
        free_list = h;
    } else {
        hdr_t *cur = free_list;
        hdr_t *next = ck_next_hdr(cur);

        while(next) {
            cur = next;
            next = ck_next_hdr(next);
        }

        cur->next = h;
    }

}

void *(ckalloc_fast)(uint32_t nbytes, src_loc_t l) {
    // Check lists to see if we can steal a block
    // NOTE: Our tests will assume they are the correct size

    // Compute log base 2 of nbytes
    int log_nbytes = 0;
    for(int i = 0; i < 32; i++) {
        if(nbytes & (1 << i))
            log_nbytes = i;
    }

    if(fixed_lists[log_nbytes]) {
        // trace("stealing block from fixed list\n");
        // Remove from head of the list
        hdr_t *h = fixed_lists[log_nbytes];
        fixed_lists[log_nbytes] = ck_next_hdr(h);
        h->state = ALLOCED;
        h->alloc_loc = l;
        h->next = 0;

        // trace("Block id %d state is %d\n", h->block_id, h->state);  

        memset(ck_data_start(h), 0, h->nbytes_alloc);

        // Add to allocated list
        if(!alloc_list) {
            alloc_list = h;
        } else {
            hdr_t *cur = ck_first_alloc();
            hdr_t *next = ck_next_hdr(cur);
    
            while(next) {
                cur = next;
                next = ck_next_hdr(next);
            }
    
            cur->next = h;
        }
        // trace("Block id %d state STILL is %d\n", h->block_id, h->state);
        void *addr = ck_data_start(h);
        // trace("Start addr %p should equal on original header %p\n", addr, ck_data_start(h));
        assert(ck_ptr_is_alloced(addr));
        return ck_data_start(h);
    }

    return ckalloc(nbytes);
}

// interpose on kr_malloc allocations and
//  1. allocate enough space for a header and fill it in.
//  2. add the allocated block to  the allocated list.
void *(ckalloc)(uint32_t nbytes, src_loc_t l) {
    static unsigned block_id=1;

    // @joshdelg NOTE: Assuming I can just add this and don't need to mess with alignment
    hdr_t *h = kr_malloc(nbytes + sizeof *h + REDZONE_NBYTES);

    // @joshdelg remove size of redzone from this
    // memset(h, 0, sizeof *h);
    memset(h, 0, sizeof *h - REDZONE_NBYTES);
    h->nbytes_alloc = nbytes;
    h->state = ALLOCED;
    h->alloc_loc = l;
    h->block_id = block_id++;

    // @joshdelg Set the first redzone
    memset(ck_get_rz1(h), REDZONE_VAL, REDZONE_NBYTES);

    // @joshdelg Set second redzone
    memset(ck_get_rz2(h), REDZONE_VAL, REDZONE_NBYTES);

    // set addr;
    void *addr = ck_data_start(h);
    assert(!ck_ptr_is_alloced(addr)); // @joshdelg Added

    // todo("put on allocated list\n");
    h->next = 0; // Just to be safe

    if(!alloc_list) {
        alloc_list = h;
    } else {
        hdr_t *cur = ck_first_alloc();
        hdr_t *next = ck_next_hdr(cur);

        while(next) {
            cur = next;
            next = ck_next_hdr(next);
        }

        cur->next = h;
    }

    assert(ck_ptr_is_alloced(addr));
    if(ck_verbose_p)
        loc_debug(l, "successful alloc of %p\n", addr);
    return addr;
}
