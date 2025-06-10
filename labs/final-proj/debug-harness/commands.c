#include "rpi.h"
#include "demand.h"

#include "strlib.h"
#include "commands.h"
#include "breakpoints.h"
#include "breakpoint.h"

static breakpoint_info_t saved_brkpts[MAX_BREAKPOINTS];

void run_program(debug_info_t *debug_info) {
    // Turn back to run state
    if(debug_info->state == DEBUG_STATE_SINGLE_STEP) {
        debug_info->state = DEBUG_STATE_RUNNING;

        brkpt_mismatch_stop();

        cp14_enable();
        // Restore breakpoints
        for(int i = 0; i < MAX_BREAKPOINTS; i++) {
            debug_info->breakpoints[i] = saved_brkpts[i];
            if(debug_info->breakpoints[i].enabled) {
                set_breakpoint_addr(debug_info->breakpoints[i].address, debug_info->breakpoints[i].type, i);
            }
        }
    }

    // Switch to target program thread, save handler state
    printk("[PI-DEBUG] Switching to target program thread\n");
    switchto_cswitch(&debug_info->handler_regs, &debug_info->target_regs);
    printk("[PI-DEBUG] Returned from target program thread\n");
}

/** Breakpoints */
uint32_t parse_breakpoint_command(debug_info_t *debug_info, char *args) {
    // Parse the command (assumes caller only extracted first token)
    char *subcommand = strtok_sp(NULL);

    printk("[PI-DEBUG] Subcommand: %s\n", subcommand);

    // Update state from step mode and restore breakpoints
    if(debug_info->state == DEBUG_STATE_SINGLE_STEP) {
        debug_info->state = DEBUG_STATE_RUNNING;
        for(int i = 0; i < MAX_BREAKPOINTS; i++) {
            debug_info->breakpoints[i] = saved_brkpts[i];
        }
    }
    
    if(strcmp(subcommand, "set") == 0) {
        char *id_str = strtok_sp(NULL);
        char *type_str = strtok_sp(NULL);
        char *addr_str = strtok_sp(NULL);

        uint32_t id = parse_breakpoint_id(id_str);
        demand(id < MAX_BREAKPOINTS, "Invalid breakpoint ID\n");

        breakpoint_type_t type = parse_breakpoint_type(type_str);
        demand(type != BREAKPOINT_INVALID, "Invalid breakpoint type\n");

        uint32_t addr = parse_breakpoint_addr(addr_str);
        demand(addr != 0, "Invalid breakpoint address\n");

        // Update debug info
        debug_info->breakpoints[id].enabled = 1;
        debug_info->breakpoints[id].type = type;
        debug_info->breakpoints[id].address = addr;

        // Update HW settings
        set_breakpoint_addr(addr, type, id);
    } else if(strcmp(subcommand, "clear") == 0) {
        char *id_str = strtok_sp(NULL);

        uint32_t id = parse_breakpoint_id(id_str);
        demand(id < MAX_BREAKPOINTS, "Invalid breakpoint ID\n");

        // Update debug info
        debug_info->breakpoints[id].enabled = 0;

        // Update HW settings
        clear_breakpoint_addr(debug_info->breakpoints[id].address, debug_info->breakpoints[id].type, id);
    } else if(strcmp(subcommand, "print") == 0) {
        printk("[PI-DEBUG] Breakpoints:\n");
        for(int i = 0; i < MAX_BREAKPOINTS; i++) {
            printk("[PI-DEBUG] Breakpoint %d %s: %s %x\n",
                i, 
                debug_info->breakpoints[i].enabled ? "enabled" : "disabled",
                debug_info->breakpoints[i].type == BREAKPOINT_MATCH ? "match" : "mismatch",
                debug_info->breakpoints[i].address);
        }
    } else {
        printk("[PI-DEBUG] Unknown breakpoint command: %s\n", subcommand);
        return 1;
    }

    return 0;
}

uint32_t parse_step_command(debug_info_t *debug_info, char *args) {
    // char *subcommand = strtok_sp(NULL); No subcommand
    // cp14_enable();
    
    // To step, we need to switch to step mode. Otherwise, correct breakpoint will be set already from handler
    if(debug_info->state != DEBUG_STATE_SINGLE_STEP) {
        debug_info->state = DEBUG_STATE_SINGLE_STEP;
        
        // Save breakpoint status so we can restore user configuration after
        if(debug_info->yap) printk("[PI-DEBUG] Saving breakpoints:\n");
        for(int i = 0; i < MAX_BREAKPOINTS; i++) {
            saved_brkpts[i] = debug_info->breakpoints[i];
        }

        // 2. Set a mismatch breakpoint on addr 0, so we fault every time
        // Starting mismatch will automatically override breakpoint 0
        if(debug_info->yap) printk("[PI-DEBUG] Setting mismatch on addr 0 with breakpoint 0\n");
        
        // We enable cp14 at init, but the staff code for mismatch expects it to be disabled
        // cp14_disable();

        // brkpt_mismatch_start();
        set_breakpoint_addr(0, BREAKPOINT_MISMATCH, 0);
        
        debug_info->breakpoints[0].type = BREAKPOINT_MISMATCH;
        debug_info->breakpoints[0].address = 0;
        debug_info->breakpoints[0].enabled = 1;

        // Disable all other breakpoints
        if(debug_info->yap) printk("[PI-DEBUG] Disabling breakpoints:\n");
        for(int i = 1; i < MAX_BREAKPOINTS; i++) {
            debug_info->breakpoints[i].enabled = 0;
            clear_breakpoint_addr(debug_info->breakpoints[i].address, debug_info->breakpoints[i].type, i);
        }
    }
    
    // 3. Switch to user program
    if(debug_info->yap) printk("[PI-DEBUG] Switching to user program\n");
    switchto_cswitch(&debug_info->handler_regs, &debug_info->target_regs);

    return 0;
}

uint32_t parse_info_command(debug_info_t *debug_info, char *args) {
    char *subcommand = strtok_sp(NULL);

    if(strcmp(subcommand, "line") == 0) {
        printk("[PI-DEBUG] Line info\n");
    } else if(strcmp(subcommand, "inst") == 0) {
        printk("[PI-DEBUG] Instruction info\n");
        printk("[PI-DEBUG] pc=%x: %x\n", debug_info->target_regs.regs[15], *(uint32_t *)(debug_info->target_regs.regs[15]));
    } else {
        printk("[PI-DEBUG] Unknown info command: %s\n", subcommand);
        return 1;
    }

    return 0;
}