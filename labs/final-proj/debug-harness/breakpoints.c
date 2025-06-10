#include "rpi.h"
#include "breakpoint.h"
#include "breakpoints.h"
#include "strlib.h"

// cp_asm_set(bvr0, p14, 0, c0, c0, 0b100);
// cp_asm_get(bvr0, p14, 0, c0, c0, 0b100);
// cp_asm_set(bvr1, p14, 0, c0, c1, 0b100);
// cp_asm_get(bvr1, p14, 0, c0, c1, 0b100);
// cp_asm_set(bvr2, p14, 0, c0, c2, 0b100);
// cp_asm_get(bvr2, p14, 0, c0, c2, 0b100);
// cp_asm_set(bvr3, p14, 0, c0, c3, 0b100);
// cp_asm_get(bvr3, p14, 0, c0, c3, 0b100);
// cp_asm_set(bvr4, p14, 0, c0, c4, 0b100);
// cp_asm_get(bvr4, p14, 0, c0, c4, 0b100);
// cp_asm_set(bvr5, p14, 0, c0, c5, 0b100);
// cp_asm_get(bvr5, p14, 0, c0, c5, 0b100);

// cp_asm_set(bcr0, p14, 0, c0, c0, 0b101);
// cp_asm_get(bcr0, p14, 0, c0, c0, 0b101);
cp_asm_set(bcr1, p14, 0, c0, c1, 0b101);
cp_asm_get(bcr1, p14, 0, c0, c1, 0b101);
// cp_asm_set(bcr2, p14, 0, c0, c2, 0b101);
// cp_asm_get(bcr2, p14, 0, c0, c2, 0b101);
// cp_asm_set(bcr3, p14, 0, c0, c3, 0b101);
// cp_asm_get(bcr3, p14, 0, c0, c3, 0b101);
// cp_asm_set(bcr4, p14, 0, c0, c4, 0b101);
// cp_asm_get(bcr4, p14, 0, c0, c4, 0b101);
// cp_asm_set(bcr5, p14, 0, c0, c5, 0b101);
// cp_asm_get(bcr5, p14, 0, c0, c5, 0b101);

// typedef void (*bvr_set_fn)(uint32_t);
// typedef void (*bcr_set_fn)(uint32_t);

// typedef uint32_t (*bvr_get_fn)(void);
// typedef uint32_t (*bcr_get_fn)(void);

// bvr_set_fn bvr_set_fns[MAX_BREAKPOINTS] = {bvr0_set, bvr1_set, bvr2_set, bvr3_set, bvr4_set, bvr5_set};
// bcr_set_fn bcr_set_fns[MAX_BREAKPOINTS] = {bcr0_set, bcr1_set, bcr2_set, bcr3_set, bcr4_set, bcr5_set};

// bvr_get_fn bvr_get_fns[MAX_BREAKPOINTS] = {bvr0_get, bvr1_get, bvr2_get, bvr3_get, bvr4_get, bvr5_get};
// bcr_get_fn bcr_get_fns[MAX_BREAKPOINTS] = {bcr0_get, bcr1_get, bcr2_get, bcr3_get, bcr4_get, bcr5_get};

// void break_match_set(uint32_t id, uint32_t addr) {
//     bvr_set_fns[id](addr);
    
//     uint32_t val = 0;
//     val = bits_set(val, 5, 8, 0b1111);
//     val = bits_set(val, 1, 2, 0b11);
//     val = bit_set(val, 0);
    
//     bcr_set_fns[id](val);
// }

// void break_match_clear(uint32_t id) {
//     uint32_t val = bcr_get_fns[id]();
//     val = bit_clr(val, 0);
//     bcr_set_fns[id](val);
// }

void brkpt_match_stop(void) {

    uint32_t bcr1_value = bcr1_get();
    bcr1_value = bit_clr(bcr1_value, 0);
    bcr1_set(bcr1_value);
}

uint32_t parse_breakpoint_id(char *id_str) {
    // Hehe final project so assume IDs are one character
    uint32_t id = id_str[0] - '0';
    
    if(id < 0 || id > MAX_BREAKPOINTS) {
        printk("Invalid breakpoint ID: %s\n", id_str);
        return MAX_BREAKPOINTS;
    }

    return id;
}

breakpoint_type_t parse_breakpoint_type(char *type_str) {
    if(strcmp(type_str, "match") == 0) {
        return BREAKPOINT_MATCH;
    } else if(strcmp(type_str, "mismatch") == 0) {
        return BREAKPOINT_MISMATCH;
    }

    return BREAKPOINT_INVALID;
}

uint32_t parse_breakpoint_addr(char *addr_str) {
    // Remove 0x prefix
    if(addr_str[0] == '0' && addr_str[1] == 'x') {
        addr_str += 2;
    } else {
        printk("Invalid breakpoint address: %s\n", addr_str);
        return 0;
    }

    return str_to_uint32(addr_str);
}

uint32_t set_breakpoint_addr(uint32_t addr, breakpoint_type_t type, uint32_t id) {
    if(type == BREAKPOINT_MATCH) {
        brkpt_match_set(addr);
        // break_match_set(id, addr);
    } else {
        // brkpt_mismatch_set(addr);
    }

    return 0;
}

uint32_t clear_breakpoint_addr(uint32_t addr, breakpoint_type_t type, uint32_t id) {
    if(type == BREAKPOINT_MATCH) {
        brkpt_match_stop();
        // break_match_clear(id);
    } else {
        // brkpt_mismatch_stop();
    }

    return 0;
}