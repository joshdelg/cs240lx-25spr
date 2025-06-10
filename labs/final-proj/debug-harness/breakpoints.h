/**
 * Wrapper around libpi/breakpoint.h that integrates with debugger
 */

#ifndef BREAKPOINTS_H
#define BREAKPOINTS_H

#include "rpi.h"

typedef enum {
    BREAKPOINT_MATCH,
    BREAKPOINT_MISMATCH,
    BREAKPOINT_INVALID
} breakpoint_type_t;

typedef struct {
    breakpoint_type_t type;
    uint32_t address;
    uint32_t enabled;
} breakpoint_info_t;

#define MAX_BREAKPOINTS 6

// Returns breakpoint ID or MAX_BREAKPOINTS on error
uint32_t parse_breakpoint_id(char *id_str);

// Returns breakpoint type or BREAKPOINT_INVALID on error
breakpoint_type_t parse_breakpoint_type(char *type_str);

// Returns breakpoint address or 0 on error
// Assumes 0x prefix
uint32_t parse_breakpoint_addr(char *addr_str);

// All functions should return 0 on success, 1 on failure
uint32_t set_breakpoint_addr(uint32_t addr, breakpoint_type_t type, uint32_t id);
uint32_t clear_breakpoint_addr(uint32_t addr, breakpoint_type_t type, uint32_t id);

// Initialize breakpoints
void break_init(void);

// Re-implementation of staff breakpoint functions
void break_match_set(uint32_t id, uint32_t addr);
void break_match_clear(uint32_t id);

void cp14_enable(void);
void cp14_disable(void);

#endif