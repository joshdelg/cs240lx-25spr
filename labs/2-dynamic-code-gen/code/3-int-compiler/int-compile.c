#include "rpi.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
#define BX_LR 0xe12fff1e
#include "cycle-util.h"

typedef void (*int_fp)(void);

static volatile unsigned cnt = 0;

// fake little "interrupt" handlers: useful just for measurement.
void int_0() { cnt++; }
void int_1() { cnt++; }
void int_2() { cnt++; }
void int_3() { cnt++; }
void int_4() { cnt++; }
void int_5() { cnt++; }
void int_6() { cnt++; }
void int_7() { cnt++; }

void generic_call_int(int_fp *intv, unsigned n) { 
    for(unsigned i = 0; i < n; i++)
        intv[i]();
}

// you will generate this dynamically.
void specialized_call_int(void) {
    int_0();
    int_1();
    int_2();
    int_3();
    int_4();
    int_5();
    int_6();
    int_7();
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

void notmain(void) {
    int_fp intv[] = {
        int_0,
        int_1,
        int_2,
        int_3,
        int_4,
        int_5,
        int_6,
        int_7
    };

    cycle_cnt_init();

    unsigned n = NELEM(intv);

    // try with and without cache: but if you modify the routines to do 
    // jump-threadig, must either:
    //  1. generate code when cache is off.
    //  2. invalidate cache before use.
    // enable_cache();

    cnt = 0;
    TIME_CYC_PRINT10("cost of generic-int calling",  generic_call_int(intv,n));
    demand(cnt == n*10, "cnt=%d, expected=%d\n", cnt, n*10);

    // rewrite to generate specialized caller dynamically.

    cnt = 0;
    TIME_CYC_PRINT10("cost of hardcoded specialized int calling", specialized_call_int() );
    demand(cnt == n*10, "cnt=%d, expected=%d\n", cnt, n*10);
    
    // Iterate over each handler
    for(int i = 0; i < NELEM(intv) - 1; i++) {
        // Iterate over each instruction
        uint32_t* inst = (uint32_t*) intv[i];

        while(*inst != BX_LR) {
            inst++;
        }
        
        *inst = armv6_b((uint32_t)inst, (uint32_t)intv[i + 1]);
        printk("MOdified bx instruction: %x\n", *inst);
    }

    cnt = 0;
    TIME_CYC_PRINT10("cost of dynamic specialized int calling", intv[0]() );
    demand(cnt == n*10, "cnt=%d, expected=%d\n", cnt, n*10);

    clean_reboot();
}
