// dynamically generate code to call a routine.  we
// do this in two steps to make it easier to debug.
//  - first, generate a call to a predefined functions 
//    that take no arguments (hello_before and hello_after).
//  - when that works, generate a call to a routine that takes
//    a 32-bit argument.  the general way that arm handles this:
//       - write the 32-bit value in the code array
//         *after* all the code you generate
//       - emit an ldr instruction using the pc to load
//         it.
#include "rpi.h"
#include "rpi-interrupts.h"

// we use this to catch if you jump off by one
static void guard1(void) { asm volatile ("bkpt"); }

static void hello1(void) { 
    printk("hello world 1\n");
}

// we use this to catch if you jump off by one
static void guard2(void) { asm volatile ("bkpt"); }

void hello2(void) { 
    printk("hello world 2\n");
}

// we use this to catch if you jump off by one
static void guard3(void) { asm volatile ("bkpt"); }

// the routines to implement.
static inline uint32_t armv6_push(int reg) {
    assert(reg<16);
    
    // 0xE9 condition and opcode fixed
    // 0x2D set write back and set written register to sp
    // Rest is one-hot register number
    return 0xE92D0000 | (1 << reg);
}

static inline uint32_t armv6_pop(int reg) {
    assert(reg<16);

    return 0xE8BD0000 | (1 << reg);
}

uint32_t pc_val_get(void) {
    uint32_t pc;
    asm volatile ("mov %0, pc" : "=r" (pc));
    return pc;
}

// pc = where the instruction will be put.  this is 
// needed so that you can compute the offset from <pc>
// to <addr> which is what gets put in <bl>
static inline uint32_t armv6_bl(uint32_t bl_pc, uint32_t target) {
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

    return 0xEB000000 | offset_24;
}

static inline uint32_t armv6_bx(uint32_t reg) {
    assert(reg<16);
    
    return 0xE12fff10 | reg;
}

static inline uint32_t 
armv6_ldr(uint32_t dst_reg, uint32_t src_reg, uint32_t off) {
    assert(dst_reg<16);
    assert(src_reg<16);

    return 0xE5900000 | src_reg << 16 | dst_reg << 12 | off;
}

// generate a dynamic call to hello() 
void jit_hello(void *fn, void * arg) {
    static uint32_t code[8];
    

    // a few of the registers
    enum {
        lr = 14,
        pc = 15,
        sp = 13,
        r0 = 0,
    };

    uint32_t addr =(uint32_t)fn;
    uint32_t n = 0;

    // generate a trampoline to call <fn>.
    //   1. we need to save and restore <lr> since the 
    //      call (bl) will trash it.
    //   2. need to make sure you sign extend <addr> in bl
    //      correctly so that it works with a negative offset!
    code[n++] = armv6_push(lr);

    // to see what value the pc register has when you read it
    // you can look at <prelab-code-pi/4-derive-pc-reg.c>
    // or also read the manual :)
    if(arg) {
        code[n++] = armv6_ldr(r0, pc, 8);
    }

    uint32_t src = (uint32_t)&code[n];
    code[n++] = armv6_bl(src, addr);
    code[n++] = armv6_pop(lr);
    code[n++] = armv6_bx(lr);
    code[n++] = (uint32_t) arg;

    printk("emitted code at %x to call routine (%x):\n", code, addr);
    for(int i = 0; i < n; i++) 
        printk("code[%d]=0x%x\n", i, code[i]);

    void (*fp)(void) = (typeof(fp))code;
    printk("about to call: %x\n", fp);
    printk("--------------------------------------\n");
    fp();
    printk("--------------------------------------\n");
}

void notmain(void) {
    // so we can catch some exceptions.
    interrupt_init();
    
    // Test push instruction
    printk("push r4: %x\n", armv6_push(4));
    printk("push lr: %x\n", armv6_push(14));
    
    // Test pop instruction
    printk("pop r4: %x\n", armv6_pop(4));
    printk("pop pc: %x\n", armv6_pop(15));
    
    printk("bl printk: %x\n", armv6_bl(0x00008048, (uint32_t) &printk));
    printk("bx lr: %x\n", armv6_bx(14));

    printk("ldr r0, [pc, #4]: %x\n", armv6_ldr(0, 15, 4));

    // step 1: generate calls that take no arguments.
    jit_hello(hello1, 0);
    jit_hello(hello2, 0);
    // step 2: generate calls that take a single argument
    jit_hello(printk, "hello world\n");
    jit_hello(printk, "hello world2\n");

    printk("bl 0x8064: %x\n", armv6_bl(0x8000, 0x8040));
}
