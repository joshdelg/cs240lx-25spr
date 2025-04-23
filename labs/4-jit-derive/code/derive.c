#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include "libunix.h"
#include <unistd.h>
#include "code-gen.h"
#include "armv6-insts.h"

# define max(a,b) (((a)>(b))?(a):(b))

/*
 *  1. emits <insts> into a temporary file: (use create_file
 *     write_exact, and then close the fd (otherwise).
 *  2. compiles it: 
 *      - look in examples/make.sh or the normal pi compilation 
 *         commands to see what commands to run.
 *      - use <run_system> to run them.
 *  3. read the results back in using <read_file> and return the pointer
 */
uint32_t *insts_emit(unsigned *nbytes, char *insts) {
    // check libunix.h --- create_file, write_exact, run_system, read_file.
    // arm-none-eabi-as test.s -o temp1 && arm-none-eabi-objcopy -O binary temp1 temp2

    // Write instructions to file
    int fd = create_file("temp.s");
    write_exact(fd, insts, strlen(insts));
    close_nofail(fd);

    // Compile instructions
    run_system("arm-none-eabi-as temp.s -o temp1");
    run_system("arm-none-eabi-objcopy -O binary temp1 temp2");

    // Read instructions back in
    uint32_t *result = read_file(nbytes, "temp2"); // n.b. will set nbytes to size of file
    return result;
}

/*
 * a cross-checking hack that uses the native GNU compiler/assembler to 
 * check our instruction encodings.
 *  1. compiles <insts> using <insts_emit>
 *  2. compares <code,nbytes> to the result of (1) for equivalance.
 *  3. prints out a useful error message if it did not succeed!!
 */
void insts_check(char *insts, uint32_t *code, unsigned nbytes) {
    // make sure you print out something useful on mismatch!
    // emit <insts>
    unsigned gen_nbytes;
    uint32_t *gen = insts_emit(&gen_nbytes, insts);

    // compare <code,nbytes> to <gen,gen_nbytes>
    if(gen_nbytes != nbytes) {
        printf("expected %d bytes, got %d from generation\n", nbytes, gen_nbytes);
    }

    if(memcmp(code, gen, nbytes) != 0) {
        printf("mismatch on instructions: %s\n", insts);
        for(int i = 0; i < nbytes / 4; i++) {
            printf("0x%x 0x%x\n", code[i], gen[i]);
        }
        exit(1);
    }
}

// check a single instruction.
void check_one_inst(char *insts, uint32_t inst) {
    return insts_check(insts, &inst, 4);
}

// helper function to make reverse engineering instructions a bit easier.
void insts_print(char *insts) {
    // emit <insts>
    unsigned gen_nbytes;
    uint32_t *gen = insts_emit(&gen_nbytes, insts);

    // print the result.
    output("getting encoding for: < %20s >\t= [", insts);
    unsigned n = gen_nbytes / 4;
    for(int i = 0; i < n; i++)
         output(" 0x%x ", gen[i]);
    output("]\n");
}


// helper function for reverse engineering.  you should refactor its interface
// so your code is better.
uint32_t emit_rrr(const char *op, const char **d, const char **s1, const char **s2, uint32_t src_to_vary, uint32_t i) {
    char buf[1024];
    
    switch (src_to_vary) {
        case 2:
            output("Varying d\n");
            sprintf(buf, "%s %s, %s, %s", op, d[i], s1[0], s2[0]);
            break;
        case 1:
            output("Varying s1\n");
            sprintf(buf, "%s %s, %s, %s", op, d[0], s1[i], s2[0]);
            break;
        case 0:
            output("Varying s2\n");
            sprintf(buf, "%s %s, %s, %s", op, d[0], s1[0], s2[i]);
            break;
    }


    uint32_t n;
    uint32_t *c = insts_emit(&n, buf);
    assert(n == 4);
    return *c;
}

// overly-specific.  some assumptions:
//  1. each register is encoded with its number (r0 = 0, r1 = 1)
//  2. related: all register fields are contiguous.
//
// NOTE: you should probably change this so you can emit all instructions 
// all at once, read in, and then solve all at once.
//
// For lab:
//  1. complete this code so that it solves for the other registers.
//  2. refactor so that you can reused the solving code vs cut and pasting it.
//  3. extend system_* so that it returns an error.
//  4. emit code to check that the derived encoding is correct.
//  5. emit if statements to checks for illegal registers (those not in <src1>,
//    <src2>, <dst>).
void derive_op_rrr(const char *name, const char *opcode, 
        const char **dst, const char **src1, const char **src2) {

    const char *s1 = src1[0];
    const char *s2 = src2[0];
    const char *d = dst[0];

    const char** srcs[] = {src2, src1, dst};
    const char* src_names[] = {"src2", "src1", "dst"};

    assert(d && s1 && s2);

    unsigned d_off = 0, src1_off = 0, src2_off = 0, op = ~0;
    unsigned *offsets[] = {&src2_off, &src1_off, &d_off};
    unsigned never_changed_all[3] = {~0, ~0, ~0};

    // compute any bits that changed as we vary d.
    for(unsigned reg_i = 0; reg_i < 3; reg_i++) {
        output("Solving for %s\n", src_names[reg_i]);
        const char** reg_src = srcs[reg_i];

        uint32_t always_0 = ~0, always_1 = ~0;

        for(unsigned i = 0; reg_src[i]; i++) {
            output("Using reg value %d: %s\n", i, reg_src[i]);
            uint32_t u = emit_rrr(opcode, dst, src1, src2, reg_i, i);

            // if a bit is always 0 then it will be 1 in always_0
            // NOTE the unary complement.
            always_0 &= ~u;

            // if a bit is always 1 it will be 1 in always_1, otherwise 0
            always_1 &= u;

            // We can and the opcode (~0) with each instruction, as we'll
            // eventually only extract the bits that nevere change
            op &= u;
        }

        if(always_0 & always_1) 
            panic("impossible overlap: always_0 = %x, always_1 %x\n", 
                always_0, always_1);

        // bits that never changed
        never_changed_all[reg_i] = always_0 | always_1;
        // bits that changed: these are the register bits.
        uint32_t changed = ~never_changed_all[reg_i];

        output("register %s are bits set in: %x\n", src_names[reg_i], changed);

        // find the offset.  we assume register bits are contig and within 0xf
        // N.B. For some reason ffs is one-indexed????
        output("offset for %s is %d\n", src_names[reg_i], ffs(changed) - 1);
        *offsets[reg_i] = ffs(changed) - 1;
        
        // check that bits are contig and at most 4 bits are set.
        if(((changed >> *offsets[reg_i]) & ~0xf) != 0)
            panic("weird instruction!  expecting at most 4 contig bits: %x\n", changed);
    }

    // refine the opcode: note until you solve for the other registers
    // this includes s1 and s2 bits

    uint32_t opcode_mask = never_changed_all[0] & never_changed_all[1] & never_changed_all[2];
    op &= opcode_mask;
    output("opcode is in =%x\n", op);

    // emit: NOTE: obviously, currently <src1_off>, <src2_off> are not 
    // defined (so solve for them) and opcode needs to be refined more.
    output("static int %s(uint32_t dst, uint32_t src1, uint32_t src2) {\n", name);
    output("    if")
    output("    return 0x%x | (dst << %d) | (src1 << %d) | (src2 << %d);\n",
                op,
                d_off,
                src1_off,
                src2_off);
    output("}\n");
}


// for(unsigned i = 0; dst[i]; i++) {
//     uint32_t u = emit_rrr(opcode, dst[i], s1, s2);

//     // if a bit is always 0 then it will be 1 in always_0
//     // NOTE the unary complement.
//     always_0 &= ~u;

//     // if a bit is always 1 it will be 1 in always_1, otherwise 0
//     always_1 &= u;
// }

// if(always_0 & always_1) 
//     panic("impossible overlap: always_0 = %x, always_1 %x\n", 
//         always_0, always_1);

// // bits that never changed
// uint32_t never_changed = always_0 | always_1;
// // bits that changed: these are the register bits.
// uint32_t changed = ~never_changed;

// output("register dst are bits set in: %x\n", changed);

// // find the offset.  we assume register bits are contig and within 0xf
// d_off = ffs(changed);

// // check that bits are contig and at most 4 bits are set.
// if(((changed >> d_off) & ~0xf) != 0)
//     panic("weird instruction!  expecting at most 4 contig bits: %x\n", changed);


// // refine the opcode: note until you solve for the other registers
// // this includes s1 and s2 bits
// op &= never_changed;
// output("opcode is in =%x\n", op);

/*
 * 1. we start by using the compiler / assembler tool chain to get / check
 *    instruction encodings.  this is sleazy and low-rent.   however, it 
 *    lets us get quick and dirty results, removing a bunch of the mystery.
 *
 * 2. after doing so we encode things "the right way" by using the armv6
 *    manual (esp chapters a3,a4,a5).  this lets you see how things are 
 *    put together.  but it is tedious.
 *
 * 3. to side-step tedium we use a variant of (1) to reverse engineer 
 *    the result.
 *
 *    we are only doing a small number of instructions today to get checked off
 *    (you, of course, are more than welcome to do a very thorough set) and focus
 *    on getting each method running from beginning to end.
 *
 * 4. then extend to a more thorough set of instructions: branches, loading
 *    a 32-bit constant, function calls.
 *
 * 5. use (4) to make a simple object oriented interface setup.
 *    you'll need: 
 *      - loads of 32-bit immediates
 *      - able to push onto a stack.
 *      - able to do a non-linking function call.
 */
int main(void) {
    // part 1: we gave the code code to do this.
    output("-----------------------------------------\n");
    output("part1: checking: correctly generating assembly.\n");
    insts_print("add r0, r0, r1");
    insts_print("bx lr");
    insts_print("mov r0, #1");
    insts_print("nop");
    output("\n");
    output("success!\n");

    // part 2: implement <check_one_inst> so these checks pass.
    // these should all pass.
    output("\n-----------------------------------------\n");
    output("part 2: checking we correctly compare asm to machine code.\n");
    check_one_inst("add r0, r0, r1", 0xe0800001);
    check_one_inst("bx lr", 0xe12fff1e);
    check_one_inst("mov r0, #1", 0xe3a00001);
    // check_one_inst("nop", 0xe320f000);
    check_one_inst("nop", 0xe1a00000);
    output("success!\n");

    // part 3: sanity check the add encoding instruction (see armv6-insts.h)
    output("\n-----------------------------------------\n");
    output("part3: checking that we can generate an <add> by hand\n");
    check_one_inst("add r0, r1, r2", arm_add(arm_r0, arm_r1, arm_r2));
    check_one_inst("add r3, r4, r5", arm_add(arm_r3, arm_r4, arm_r5));
    check_one_inst("add r6, r7, r8", arm_add(arm_r6, arm_r7, arm_r8));
    check_one_inst("add r9, r10, r11", arm_add(arm_r9, arm_r10, arm_r11));
    check_one_inst("add r12, r13, r14", arm_add(arm_r12, arm_r13, arm_r14));
    check_one_inst("add r15, r7, r3", arm_add(arm_r15, arm_r7, arm_r3));
    output("success!\n");

    // part 4: implement the code so it will derive the add instruction.
    output("\n-----------------------------------------\n");
    output("part4: checking that we can reverse engineer an <add>\n");

    const char *all_regs[] = {
                "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
                "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
                0 
    };

    // XXX: should probably pass a bitmask in instead.
    derive_op_rrr("arm_add", "add", all_regs,all_regs,all_regs);
    output("did something: now use the generated code in the checks above!\n");

    // NOW:
    //   1. get the encodings for other instructions you used.
    //   2. write a routine that will check all the register
    //      permutations.
    //   3. write code to automatically emit the instruction encoder.
    return 0;
}
