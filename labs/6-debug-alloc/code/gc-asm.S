#include "rpi-asm.h"

@ trampoline used to:
@   - save all callee saved registers to stack.
@   - call ck_gc_fn(sp)
@   - return
@
@  note: also have to save/restore lr
MK_FN(ck_gc)
    # asm_todo("implement this\n");

    # r4-r8, r10, r11
    push {r4-r11}
    mov r0, sp
    push {lr}
    bl ck_gc_fn

    pop {lr}
    pop {r4-r11}
    
    bx lr


@ similar trampoline as above, but will call
@ <ck_find_leaks_fn>.  note: sp is in a different
@ register
MK_FN(ck_find_leaks)
    # asm_todo("implement this\n");
    push {r4-r11}
    mov r1, sp
    push {lr}
    bl ck_find_leaks_fn

    pop {lr}
    pop {r4-r11}

    bx lr

