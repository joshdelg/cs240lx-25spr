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

        // brkpt_mismatch_stop();

        // cp14_enable();

        // Disable the mismatch breakpoint
        clear_breakpoint_addr(0, BREAKPOINT_MISMATCH, 0);

        // Restore breakpoints
        for(int i = 1; i < MAX_BREAKPOINTS; i++) {
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
uint32_t parse_breakpoint_command(debug_info_t *debug_info, char *args, source_ctx_t *source_ctx) {
    // Parse the command (assumes caller only extracted first token)
    char *subcommand = strtok_sp(NULL);

    printk("[PI-DEBUG] Subcommand: %s\n", subcommand);

    // Update state from step mode and restore breakpoints
    if(debug_info->state == DEBUG_STATE_SINGLE_STEP) {
        debug_info->state = DEBUG_STATE_RUNNING;
        
        // Clear single step breakpoint
        clear_breakpoint_addr(0, BREAKPOINT_MISMATCH, 0);
        
        for(int i = 1; i < MAX_BREAKPOINTS; i++) {
            debug_info->breakpoints[i] = saved_brkpts[i];
            if(debug_info->breakpoints[i].enabled) {
                set_breakpoint_addr(debug_info->breakpoints[i].address, debug_info->breakpoints[i].type, i);
            }
        }
    }
    
    if(strcmp(subcommand, "set") == 0) {
        char *id_str = strtok_sp(NULL);
        char *type_str = strtok_sp(NULL);
        char *addr_or_flag = strtok_sp(NULL);

        uint32_t id = parse_breakpoint_id(id_str);
        demand(id < MAX_BREAKPOINTS, "Invalid breakpoint ID\n");

        breakpoint_type_t type = parse_breakpoint_type(type_str);
        demand(type != BREAKPOINT_INVALID, "Invalid breakpoint type\n");

        uint32_t addr = 0;

        // Find addr if a filename and line number is passed
        if(strcmp(addr_or_flag, "-f") == 0) {
            char *file_str = strtok_sp(NULL);
            char *lineno_str = strtok_sp(NULL);
            uint32_t lineno = str_to_uint32(lineno_str);

            // Search the line map for the filename and line number
            for(int i = 0; i < source_ctx->n_line_map; i++) {
                if(str_ends_with(source_ctx->line_map_strtab + source_ctx->line_map[i].filename, file_str) && source_ctx->line_map[i].line_num == lineno) {
                    addr = source_ctx->line_map[i].start_addr;
                    printk("[PI-DEBUG] Found file %s line %d at %x\n", file_str, lineno, addr);
                    break;
                }
            }
        } else {
            char *addr_str = addr_or_flag;
            addr = parse_breakpoint_addr(addr_str);
        }

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

uint32_t parse_info_command(debug_info_t *debug_info, char *args, source_ctx_t *source_ctx) {
    char *subcommand = strtok_sp(NULL);

    if(strcmp(subcommand, "line") == 0) {
        printk("[PI-DEBUG] Line Info\n");

        uint32_t pc = debug_info->target_regs.regs[15];

        // Search the line map for an entry with start_addr <= pc < end_addr
        uint32_t line_map_entry = 0;
        for(int i = 0; i < source_ctx->n_line_map; i++) {
            if(source_ctx->line_map[i].start_addr <= pc && pc <= source_ctx->line_map[i].end_addr) {
                line_map_entry = i;
                break;
            }
        }

        if(line_map_entry == 0) {
            printk("[PI-DEBUG] No line map entry found for pc=%x\n", pc);
        } else {
            printk("[PI-DEBUG] pc=%x: %s:%d\n", pc, source_ctx->line_map_strtab + source_ctx->line_map[line_map_entry].filename, source_ctx->line_map[line_map_entry].line_num);

            // If the file is hello.c, print surrounding context
            if(strcmp(source_ctx->line_map_strtab + source_ctx->line_map[line_map_entry].filename, "/Users/joshdelg/cs240lx-25spr/labs/final-proj/compile/hello.c") == 0) {
                printk("[PI-DEBUG] Current location in hello.c:\n");
                
                // Print up to 5 lines before
                int start_line = source_ctx->line_map[line_map_entry].line_num - 5;
                if(start_line < 1) start_line = 1;
                
                // Print up to 5 lines after 
                int end_line = source_ctx->line_map[line_map_entry].line_num + 5;
                
                for(int line = start_line; line <= end_line; line++) {
                    if(line == source_ctx->line_map[line_map_entry].line_num) {
                        printk("=> %d: %s\n", line, source_ctx->source_map[line-1]);
                    } else {
                        printk("   %d: %s\n", line, source_ctx->source_map[line-1]); 
                    }
                }
            }
        }
    } else if(strcmp(subcommand, "inst") == 0) {
        printk("[PI-DEBUG] Instruction Info\n");

        uint32_t pc = debug_info->target_regs.regs[15];
        uint32_t offset = (pc - 0x50000) / 4;

        printk("[PI-DEBUG] Current location in assembly:\n");
        
        // Print up to 5 instructions before
        int start_inst = offset - 5;
        if(start_inst < 0) start_inst = 0;
        
        // Print up to 5 instructions after
        int end_inst = offset + 5;
        
        for(int inst = start_inst; inst <= end_inst; inst++) {
            uint32_t inst_addr = 0x50000 + (inst * 4);
            if(inst == offset) {
                printk("=> %x: %s\n", inst_addr, source_ctx->source_map_asm[inst]);
            } else {
                printk("   %x: %s\n", inst_addr, source_ctx->source_map_asm[inst]);
            }
        }
    } else {
        printk("[PI-DEBUG] Unknown info command: %s\n", subcommand);
        return 1;
    }

    return 0;
}