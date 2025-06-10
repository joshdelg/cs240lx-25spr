#include "rpi.h"
#include "cpsr-util.h"

// 140e breakpoint support.
#include "breakpoint.h"
// 140e exception handling support
#include "full-except.h"
// 140e helpers for getting exception reason.
#include "armv6-except.h"
// 140e code for full context switching
// (caller,callee and cpsr).
#include "switchto.h"

#include "strlib.h"
#include "commands.h"
#include "breakpoints.h"

static debug_info_t debug_info = {
    .state = DEBUG_STATE_RUNNING,
    .breakpoints = {
        {.type = BREAKPOINT_MATCH, .address = 0, .enabled = 0},
        {.type = BREAKPOINT_MATCH, .address = 0, .enabled = 0},
        {.type = BREAKPOINT_MATCH, .address = 0, .enabled = 0},
        {.type = BREAKPOINT_MATCH, .address = 0, .enabled = 0},
        {.type = BREAKPOINT_MATCH, .address = 0, .enabled = 0},
        {.type = BREAKPOINT_MATCH, .address = 0, .enabled = 0},
    },
    .handler_regs = {},
    .target_regs = {},
    .yap = 1
};

// Returns the length of command (not including newline/null)
uint32_t get_command(char *command_buffer) {
    int i = 0;
    while((command_buffer[i] = uart_get8()) != '\n') {
        i++;
    }
    command_buffer[i] = '\0';

    return i;
}

uint32_t dispatch_command(char *command) {
    char *cmd_name = strtok_sp(command);
    
    if(cmd_name != NULL) {
        printk("[PI-DEBUG] Command: %s\n", cmd_name);
    } else {
        printk("[PI-DEBUG] No command found\n");
        return -1;
    }

    // @joshdelg I don't know why, but I need to flush UART before we branch
    // I imagine this is becuase UART is initialized in debug harness, and
    // then again in the debug target ??
    uart_flush_tx();

    if (strcmp(cmd_name, "run") == 0) {
        run_program(&debug_info);
    } else if (strcmp(cmd_name, "break") == 0) {
        parse_breakpoint_command(&debug_info, cmd_name);
    } else if (strcmp(cmd_name, "step") == 0) {
        parse_step_command(&debug_info, cmd_name);
    } else if (strcmp(cmd_name, "info") == 0) {
        parse_info_command(&debug_info, cmd_name);
    } else {
        printk("[PI-DEBUG] Unknown command: %s\n", cmd_name);
        return -1;
    }

    return 0;
}

static void debug_target_done(void) {
    printk("[PI-DEBUG] Debug target code finished executing. Returning to handler.\n");

    // Doesn't work because our thread is in user mode
    // switchto_cswitch(&debug_info.target_regs, &debug_info.handler_regs);
    switchto(&debug_info.handler_regs);
}

// initializes the full register set so it can be
// run on its own.
static regs_t 
// thread_mk(void *fn, uint32_t arg, uint32_t stack_p) {
thread_mk(void *fn, uint32_t stack_p) {
    // compute USER cpsr using current cpsr.
    uint32_t cpsr = cpsr_inherit(USER_MODE, cpsr_get());

    // Make it a SUPER thread
    // uint32_t cpsr = cpsr_inherit(SUPER_MODE, cpsr_get());

    // statically allocate stack.
    static  __attribute__ ((aligned(8))) uint64_t stack[1024*4];
    uint32_t sp = 0;
    // stack grows down, so stack pointer = 
    //  stack_base + nbytes.
    if(stack_p)
        sp = (uint32_t)stack + sizeof stack;

    // initialize the registers
    //  see <switchto.h>
    return (regs_t) {
        .regs[REGS_PC] = (uint32_t)fn,
        // the first argument to fn
        // .regs[REGS_R0] = arg,

        // stack pointer register
        .regs[REGS_SP] = sp,
        // the cpsr to use
        .regs[REGS_CPSR] = cpsr,

        // where to jump to if the code returns.
        // see <single-step-start.S>.  note: if you
        // want to change how to stop single step
        // execution (syscall, special pc, illegal
        // instructions) you where the code jumps.
        .regs[REGS_LR] = (uint32_t)debug_target_done,
    };
}

void prefetch_handler(regs_t *r) {
    printk("[PI-DEBUG] Prefetch handler\n");

    // make sure it was a breakpoint fault.
    if(!brkpt_fault_p())
        panic("have a non-breakpoint fault\n");
    
    printk("[PI-DEBUG] Breakpoint fault!\n");

    // get the pc and cpsr from the saved regs.
    uint32_t pc = r->regs[15];
    uint32_t cpsr = r->regs[16];

    printk("[PI-DEBUG] PC: %x, CPSR: %x\n", pc, cpsr);

    // if(debug_info.state == DEBUG_STATE_SINGLE_STEP) {
    //     // Set a mismatch breakpoint on faulting address to single step
    //     if(debug_info.yap) printk("[PI-DEBUG] Setting mismatch breakpoint on faulting address: %x\n", pc);
    //     set_breakpoint_addr(pc, BREAKPOINT_MISMATCH, 0);
    //     debug_info.breakpoints[0].type = BREAKPOINT_MISMATCH;
    //     debug_info.breakpoints[0].address = pc;
    //     debug_info.breakpoints[0].enabled = 1;
    // }

    // brkpt_mismatch_set(pc);
    if(debug_info.state == DEBUG_STATE_SINGLE_STEP) {
        set_breakpoint_addr(pc, BREAKPOINT_MISMATCH, 0);
    }

    // Save return regs
    // debug_info.resume_regs = *r;

    // Branch back to 

    // n_inst++;

    // Option 1: Turn off breakpoint and resume
    // brkpt_match_stop();
    // break_match_clear(0);
    // break_init();

    // 
    
    // switchto(r);

    // We're still in the debug target thread, so we need to switch back to the handler
    // However, we actually want to set the debug target's regs to where the breakpoint was
    // Not this handler code
    debug_info.target_regs = *r;
    // switchto_cswitch(&debug_info.target_regs, &debug_info.handler_regs);

    printk("[PI-DEBUG] Target PC: %x\n", debug_info.target_regs.regs[15]);
    printk("[PI-DEBUG] Handler PC: %x\n", debug_info.handler_regs.regs[15]);

    switchto(&debug_info.handler_regs);
}

// We use this to spawn a user-level thread for the debugging target
// It starts with PC = Entrypoint, and is invokved whenever we see the "run" command
// We use cswitch to start the thread and save the handler's state
// We set the lr to a small trampoline which reports finished and switches back to handler
// When a breakpoint is hit, we save the update debugging target regs, then switch back to handler



void notmain(void) {
    printk("Starting debugger!\n");

    /** Set up exeception modes: breakpoints, single-stepping */
    if(mode_get(cpsr_get()) != SUPER_MODE)
        panic("not at SUPER level?\n");

    full_except_install(0);
    full_except_set_prefetch(prefetch_handler);

    // brkpt_match_init();
    break_init();

    debug_info.target_regs = thread_mk((void *)TARGET_ENTRYPOINT, 1024*4);

    while(1) {
        printk("[PI-DEBUG] RPi Debugger - pc=%x: %x\n", debug_info.target_regs.regs[15], *(uint32_t *)(debug_info.target_regs.regs[15]));
        printk("> ");
        
        char command[MAX_COMMAND_LENGTH];
        uint32_t len = get_command(command);

        uint32_t ret = dispatch_command(command);
        if(ret == -1) {
            printk("[PI-DEBUG] Error dispatching command\n");
        }
    }
}