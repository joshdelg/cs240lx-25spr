#ifndef COMMANDS_H
#define COMMANDS_H

#include "breakpoints.h"
#include "switchto.h"

#define TARGET_OFFSET 0x50000
#define TARGET_ENTRYPOINT 0x50050 // Addr of notmain() -- because we already run cstart

typedef enum {
    DEBUG_STATE_RUNNING,
    DEBUG_STATE_SINGLE_STEP
} debug_state_t;

typedef struct {
    debug_state_t state;
    breakpoint_info_t breakpoints[6];
    regs_t handler_regs;
    regs_t target_regs;
    uint32_t yap;
} debug_info_t;

void run_program();

/** Breakpoints */

// break [set] [id] [type] [addr]
// break [clear] [id]
// break [print]
uint32_t parse_breakpoint_command(debug_info_t *debug_info, char *args);
uint32_t find_first_disabled_breakpoint(debug_info_t *debug_info);

// step
uint32_t parse_step_command(debug_info_t *debug_info, char *args);

/** Information */

// info line
// info inst
uint32_t parse_info_command(debug_info_t *debug_info, char *args);

#endif