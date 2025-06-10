#include "rpi.h"
#include "demand.h"

#include "strlib.h"
#include "commands.h"
#include "breakpoints.h"

void run_program(debug_info_t *debug_info) {
    // Switch to target program thread, save handler state
    printk("Switching to target program thread\n");
    switchto_cswitch(&debug_info->handler_regs, &debug_info->target_regs);
    printk("Returned from target program thread\n");
}

/** Breakpoints */
uint32_t parse_breakpoint_command(debug_info_t *debug_info, char *args) {
    // Parse the command (assumes caller only extracted first token)
    char *subcommand = strtok_sp(NULL);

    printk("Subcommand: %s\n", subcommand);

    
    if(strcmp(subcommand, "set") == 0) {
        char *id_str = strtok_sp(NULL);
        printk("ID: %s\n", id_str);

        char *type_str = strtok_sp(NULL);
        printk("Type: %s\n", type_str);

        char *addr_str = strtok_sp(NULL);
        printk("Addr: %s\n", addr_str);

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

        printk("updated debug info\n");

        // Update HW settings
        set_breakpoint_addr(addr, type, id);

        printk("updated hw settings\n");

    } else if(strcmp(subcommand, "clear") == 0) {
        char *id_str = strtok_sp(NULL);

        uint32_t id = parse_breakpoint_id(id_str);
        demand(id < MAX_BREAKPOINTS, "Invalid breakpoint ID\n");

        // Update debug info
        debug_info->breakpoints[id].enabled = 0;

        // Update HW settings
        clear_breakpoint_addr(debug_info->breakpoints[id].address, debug_info->breakpoints[id].type, id);
    } else if(strcmp(subcommand, "print") == 0) {
        printk("Breakpoints:\n");
        for(int i = 0; i < MAX_BREAKPOINTS; i++) {
            printk("Breakpoint %d %s: %s %x\n",
                i, 
                debug_info->breakpoints[i].enabled ? "enabled" : "disabled",
                debug_info->breakpoints[i].type == BREAKPOINT_MATCH ? "match" : "mismatch",
                debug_info->breakpoints[i].address);
        }
    } else {
        printk("Unknown breakpoint command: %s\n", subcommand);
        return 1;
    }

    return 0;
}