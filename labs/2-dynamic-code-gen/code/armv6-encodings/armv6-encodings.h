#ifndef __ARMV6_ENCODINGS_H__
#define __ARMV6_ENCODINGS_H__

#define abs(x) ((x) < 0 ? -(x) : (x))

enum {
    armv6_lr = 14,
    armv6_pc = 15,
    armv6_sp = 13,
    armv6_r0 = 0,

    op_mov = 0b1101,
    armv6_mvn = 0b1111,
    armv6_orr = 0b1100,
    op_mult = 0b0000,

    cond_always = 0b1110
};

// trivial wrapper for register values so type-checking works better.
typedef struct {
    uint8_t reg;
} reg_t;
static inline reg_t reg_mk(unsigned r) {
    if(r >= 16)
        panic("illegal reg %d\n", r);
    return (reg_t){ .reg = r };
}

// how do you add a negative? do a sub?
static inline uint32_t armv6_mov(reg_t rd, reg_t rn) {
    // could return an error.  could get rid of checks.
    //         I        opcode        | rd
    return cond_always << 28
        | op_mov << 21 
        | rd.reg << 12 
        | rn.reg
        ;
}

static inline uint32_t 
armv6_mov_imm8_rot4(reg_t rd, uint32_t imm8, unsigned rot4) {
    if(imm8>>8)
        panic("immediate %d does not fit in 8 bits!\n", imm8);
    if(rot4 % 2)
        panic("rotation %d must be divisible by 2!\n", rot4);
    rot4 /= 2;
    if(rot4>>4)
        panic("rotation %d does not fit in 4 bits!\n", rot4);

    // todo("implement mov with rotate\n");

    return cond_always << 28 | 0b001 << 25 | op_mov << 21 | 0 << 20 | 0b0000 << 16 | rd.reg << 12 | rot4 << 8 | imm8;
}
static inline uint32_t 
armv6_mov_imm8(reg_t rd, uint32_t imm8) {
    return armv6_mov_imm8_rot4(rd,imm8,0);
}

static inline uint32_t 
armv6_mvn_imm8(reg_t rd, uint32_t imm8) {
    todo("implement mvn\n");
}

static inline uint32_t 
armv6_bx(reg_t rd) {
    return 0xE12fff10 | rd.reg;
}

static inline uint32_t armv6_b(uint32_t bl_pc, uint32_t target) {
    // Do the steps that transform the 24 bit immediate to the
    // PC-relative address in reverse
    // printk("bl_pc: %x\n", bl_pc);
    // printk("targetL %x\n", target);

    // 3. bl_pc + 8 + offset = target
    uint32_t offset = target - bl_pc - 8;
    // printk("after bl_pc-8: %x\n", offset);

    // 2. Shift result left 2 bits to make it 32
    uint32_t offset_30 = offset >> 2;
    // printk("after shift: %x\n", offset_30);

    // 1. Sign extend 24 two's complement to 30 bits
    uint32_t offset_24 = offset_30 & 0x00FFFFFF;
    // printk("afer reverse extension: %x\n", offset_24);

    return 0xEA000000 | offset_24;
}

// use 8-bit immediate imm8, with a 4-bit rotation.
static inline uint32_t 
armv6_orr_imm8_rot4(reg_t rd, reg_t rn, unsigned imm8, unsigned rot4) {
    if(imm8>>8)
        panic("immediate %d does not fit in 8 bits!\n", imm8);
    if(rot4 % 2)
        panic("rotation %d must be divisible by 2!\n", rot4);
    rot4 /= 2;
    if(rot4>>4)
        panic("rotation %d does not fit in 4 bits!\n", rot4);

    return cond_always << 28 | 0b001 << 25 | armv6_orr << 21 | 0 << 20 | rn.reg << 16 | rd.reg << 12 | rot4 << 8 | imm8;
}

static inline uint32_t 
armv6_orr_imm8(reg_t rd, reg_t rn, unsigned imm8) {
    if(imm8>>8)
        panic("immediate %d does not fit in 8 bits!\n", imm8);

    // todo("implement orr with immediate\n");
    return armv6_orr_imm8_rot4(rd,rn,imm8,0);
}

// a4-80
static inline uint32_t 
armv6_mult(reg_t rd, reg_t rm, reg_t rs) {
    todo("implement mult\n");
}


// load a word from memory[offset]
// ldr rd, [rn,#offset]
static inline uint32_t 
armv6_ldr_off12(reg_t rd, reg_t rn, int offset) {
    // a5-20
    // todo("implement lrd_off12\n");

    return cond_always << 28 | 0b0101 << 24 | (offset >= 0 ? 0b1 : 0b0) << 23 | 0b001 << 20 | rn.reg << 16 | rd.reg << 12 | abs(offset);
}

/**********************************************************************
 * synthetic instructions.
 * 
 * these can result in multiple instructions generated so we have to 
 * pass in a location to store them into.
 */

// Return the next free addr at the code pointer
static inline uint32_t *
armv6_load_imm32(uint32_t *code, reg_t rd, uint32_t imm32) {
    // todo("implement loading arbitrary constant\n");

    // 0: load reg
    // 1: branch over value
    // 2: value

    // pc is currently 8 ahead of current inst, that is already the value to load
    code[0] = armv6_ldr_off12(rd, reg_mk(armv6_pc), 0);
    code[1] = armv6_b((unsigned) (code + 1), (unsigned) (code + 3));
    code[2] = imm32;

    return code + 3;
}

// MLA (Multiply Accumulate) multiplies two signed or unsigned 32-bit
// values, and adds a third 32-bit value.
//
// a4-66: multiply accumulate.
//      rd = rm * rs + rn.
static inline uint32_t
armv6_mla(reg_t rd, reg_t rm, reg_t rs, reg_t rn) {    
    return cond_always << 28 | 0b00000010 << 20 | rd.reg << 16 | rn.reg << 12 | rs.reg << 8 | 0b1001 << 4 | rm.reg;
}

#endif
