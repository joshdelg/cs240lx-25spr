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

#include "pi-sd.h"
#include "fat32.h"

#define SECTOR_SIZE 512

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

static char **source_map = NULL;
static uint32_t n_source_lines = 0;

static char** source_map_asm = NULL;
static uint32_t n_source_lines_asm = 0;

static line_map_t *line_map = NULL;
static uint32_t n_line_map = 0;

static char *line_map_strtab = NULL;
static uint32_t n_line_map_strtab = 0;

// Returns the length of command (not including newline/null)
uint32_t get_command(char *command_buffer) {
    int i = 0;
    while((command_buffer[i] = uart_get8()) != '\n') {
        i++;
    }
    command_buffer[i] = '\0';

    return i;
}

uint32_t dispatch_command(char *command, source_ctx_t *source_ctx) {
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
        parse_breakpoint_command(&debug_info, cmd_name, source_ctx);
    } else if (strcmp(cmd_name, "step") == 0) {
        parse_step_command(&debug_info, cmd_name);
    } else if (strcmp(cmd_name, "info") == 0) {
        parse_info_command(&debug_info, cmd_name, source_ctx);
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
    // if(!brkpt_fault_p())
    //     panic("have a non-breakpoint fault\n");
    
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

char **build_source_map(char *source_file, char *data, uint32_t n_data, uint32_t *n_lines_out) {
    printk("Building source map for %s\n", source_file);

    // Count number of lines by counting newlines
    uint32_t n_lines = 1; // Start at 1 for last line
    for(uint32_t i = 0; i < n_data; i++) {
        if(data[i] == '\n') {
            n_lines++;
        }
    }

    // Allocate array of char* for each line
    char **lines = kmalloc(n_lines * sizeof(char*));
    
    // Track current line and position within it
    uint32_t curr_line = 0;
    uint32_t line_start = 0;
    uint32_t line_len = 0;

    // Split into lines
    for(uint32_t i = 0; i <= n_data; i++) {
        if(i == n_data || data[i] == '\n') {
            // Allocate space for line + null terminator
            lines[curr_line] = kmalloc(line_len + 1);
            
            // Copy line contents
            memcpy(lines[curr_line], data + line_start, line_len);
            lines[curr_line][line_len] = '\0';

            // Reset for next line
            curr_line++;
            line_start = i + 1;
            line_len = 0;
        } else {
            line_len++;
        }
    }

    *n_lines_out = n_lines;
    return lines;
}

char** build_source_map_asm(char *source_file, char *data, uint32_t n_data, uint32_t *n_lines_out) {
    printk("Building ASM source map for %s\n", source_file);

    // Count number of lines by counting newlines
    uint32_t n_lines = 1; // Start at 1 for last line
    for(uint32_t i = 0; i < n_data; i++) {
        if(data[i] == '\n') {
            n_lines++;
        }
    }

    // Allocate array of char* for each word offset from 0x50000
    char **lines = kmalloc(n_lines * sizeof(char*));

    // Track current line and position
    uint32_t curr_line = 0;
    uint32_t line_start = 0;
    uint32_t line_len = 0;
    uint32_t offset;

    // Split into lines and extract assembly
    for(uint32_t i = 0; i <= n_data; i++) {
        if(i == n_data || data[i] == '\n') {
            // Skip empty lines
            if(line_len == 0) {
                line_start = i + 1;
                continue;
            }

            // Find end of address portion (before colon)
            uint32_t addr_end = line_start;
            while(addr_end < i && data[addr_end] != ':') {
                addr_end++;
            }

            // Extract and parse address
            uint32_t addr_len = addr_end - line_start;
            char *addr_str = kmalloc(addr_len + 1);
            memcpy(addr_str, data + line_start, addr_len);
            addr_str[addr_len] = '\0';
            
            // Convert hex string to int and get word offset from 0x50000
            offset = (str_to_uint32(addr_str) - 0x50000) / 4;
            
            // kfree(addr_str);

            // Skip colon and space to get to instruction
            uint32_t asm_start = addr_end + 2;

            // Calculate assembly length
            uint32_t asm_len = i - asm_start;

            // Allocate and copy assembly string
            lines[offset] = kmalloc(asm_len + 1);
            memcpy(lines[offset], data + asm_start, asm_len);
            lines[offset][asm_len] = '\0';

            // Reset for next line
            curr_line++;
            line_start = i + 1;
            line_len = 0;
        } else {
            line_len++;
        }
    }

    *n_lines_out = n_lines;
    return lines;
}

void build_line_map(char *source_file, char *data, uint32_t n_data) {
    printk("Building line map for %s\n", source_file);

    // Count unique strings for string table
    char **unique_strs = kmalloc(1024 * sizeof(char*)); // Temporary array for unique strings
    int n_unique = 0;
    int total_str_len = 0;

    // Parse each line
    int line_start = 0;
    int line_len = 0;
    int n_lines = 0;

    // First pass - count lines and collect unique strings
    for(int i = 0; i < n_data; i++) {
        if(data[i] == '\n') {
            if(line_len == 0) {
                line_start = i + 1;
                continue;
            }

            // Extract tokens from line
            char *line = kmalloc(line_len + 1);
            memcpy(line, data + line_start, line_len);
            line[line_len] = '\0';

            char *filename = strtok_sp(line);
            char *line_num_str = strtok_sp(NULL);
            char *funcname = strtok_sp(NULL);
            char *start_addr_str = strtok_sp(NULL); 
            char *end_addr_str = strtok_sp(NULL);

            // Add filename if unique
            int found = 0;
            for(int j = 0; j < n_unique; j++) {
                if(strcmp(unique_strs[j], filename) == 0) {
                    found = 1;
                    break;
                }
            }
            if(!found) {
                char *str_copy = kmalloc(strlen(filename) + 1);
                strcpy(str_copy, filename);
                unique_strs[n_unique] = str_copy;
                total_str_len += strlen(filename) + 1;
                n_unique++;
            }

            // Add funcname if unique
            found = 0;
            for(int j = 0; j < n_unique; j++) {
                if(strcmp(unique_strs[j], funcname) == 0) {
                    found = 1;
                    break;
                }
            }
            if(!found) {
                char *str_copy = kmalloc(strlen(funcname) + 1);
                strcpy(str_copy, funcname);
                unique_strs[n_unique] = str_copy;
                total_str_len += strlen(funcname) + 1;
                n_unique++;
            }

            // kfree(line);
            n_lines++;
            line_start = i + 1;
            line_len = 0;
        } else {
            line_len++;
        }
    }

    // Allocate string table and line map
    char *str_table = kmalloc(total_str_len);
    line_map_t *line_map_local = kmalloc(n_lines * sizeof(line_map_t));

    // Build string table
    int str_offset = 0;
    for(int i = 0; i < n_unique; i++) {
        int len = strlen(unique_strs[i]) + 1;
        memcpy(str_table + str_offset, unique_strs[i], len);
        str_offset += len;
    }
    
    line_map_strtab = str_table;
    n_line_map_strtab = total_str_len;

    // Second pass - build line map entries
    line_start = 0;
    line_len = 0;
    int curr_line = 0;

    for(int i = 0; i < n_data; i++) {
        if(data[i] == '\n') {
            if(line_len == 0) {
                line_start = i + 1;
                continue;
            }

            char *line = kmalloc(line_len + 1);
            memcpy(line, data + line_start, line_len);
            line[line_len] = '\0';

            // printk("Line: %s\n", line);

            char *filename = strtok_sp(line);
            char *line_num_str = strtok_sp(NULL);
            char *funcname = strtok_sp(NULL);

            if(strncmp(funcname, "(discriminator", 13) == 0) {
                char *x = strtok_sp(NULL);
                funcname = strtok_sp(NULL);
            }

            char *start_addr_str = strtok_sp(NULL);
            char *end_addr_str = strtok_sp(NULL);

            // Find filename offset in string table
            str_offset = 0;
            while(strcmp(str_table + str_offset, filename) != 0) {
                str_offset += strlen(str_table + str_offset) + 1;
            }
            line_map_local[curr_line].filename = str_offset;

            // Find funcname offset in string table
            str_offset = 0;
            while(strcmp(str_table + str_offset, funcname) != 0) {
                str_offset += strlen(str_table + str_offset) + 1;
            }
            line_map_local[curr_line].funcname = str_offset;

            uint32_t line_num = str_to_uint32(line_num_str);
            uint32_t start_addr = str_to_uint32(start_addr_str); 
            uint32_t end_addr = str_to_uint32(end_addr_str);

            if(line_num && start_addr && end_addr) {
                line_map_local[curr_line].line_num = line_num;
                line_map_local[curr_line].start_addr = start_addr;
                line_map_local[curr_line].end_addr = end_addr;
            }

            // kfree(line);
            curr_line++;
            line_start = i + 1;
            line_len = 0;
        } else {
            line_len++;
        }
    }

    // // Free temporary storage
    // for(int i = 0; i < n_unique; i++) {
    //     kfree(unique_strs[i]);
    // }
    // kfree(unique_strs);

    line_map = line_map_local;
    n_line_map = n_lines;
}

void notmain(void) {

    kmalloc_init(FAT32_HEAP_MB);
    pi_sd_init();

    printk("Reading the MBR.\n");
    mbr_t *mbr = mbr_read();

    printk("Loading the first partition.\n");
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));

    printk("Loading the FAT.\n");
    fat32_fs_t fs = fat32_mk(&partition);

    printk("Loading the root directory.\n");
    pi_dirent_t root = fat32_get_root(&fs);

    printk("Listing files:\n");
    uint32_t n;
    pi_directory_t files = fat32_readdir(&fs, &root);
    printk("Got %d files.\n", files.ndirents);
    for (int i = 0; i < files.ndirents; i++) {
        if (files.dirents[i].is_dir_p) {
        printk("\tD: %s (cluster %d)\n", files.dirents[i].name, files.dirents[i].cluster_id);
        } else {
        printk("\tF: %s (cluster %d; %d bytes)\n", files.dirents[i].name, files.dirents[i].cluster_id, files.dirents[i].nbytes);
        }
    }

    /** Build source map for .C file */
    char *source_file = files.dirents[20].name; // Hardcoded to HELLO.C -- idk why the string wasn't working
    pi_dirent_t *source = fat32_stat(&fs, &root, source_file);
    demand(source, "HELLO.C not found!\n");

    pi_file_t *file = fat32_read(&fs, &root, source_file);

    source_map = build_source_map(source_file, file->data, file->n_data, &n_source_lines);

    /** Build source map for .list file */
    char *list_file_name = files.dirents[22].name;
    pi_dirent_t *list_file_dirent = fat32_stat(&fs, &root, list_file_name);
    demand(list_file_dirent, "HELLO.MAP not found!\n");

    pi_file_t *list_file = fat32_read(&fs, &root, list_file_name);

    source_map_asm = build_source_map_asm(list_file_name, list_file->data, list_file->n_data, &n_source_lines_asm);

    /** Build line map */
    char *line_map_file_name = "HELL~33.LIN";
    pi_dirent_t *line_map_file_dirent = fat32_stat(&fs, &root, line_map_file_name);
    demand(line_map_file_dirent, "HELLO.LIN not found!\n");
    
    pi_file_t *line_map_file = fat32_read(&fs, &root, line_map_file_name);
    build_line_map(line_map_file_name, line_map_file->data, line_map_file->n_data);

    /** Set up exeception modes: breakpoints, single-stepping */
    if(mode_get(cpsr_get()) != SUPER_MODE)
        panic("not at SUPER level?\n");

    full_except_install(0);
    full_except_set_prefetch(prefetch_handler);

    // brkpt_match_init();
    break_init();

    debug_info.target_regs = thread_mk((void *)TARGET_ENTRYPOINT, 1024*4);

    source_ctx_t source_ctx = {
        .line_map = line_map,
        .line_map_strtab = line_map_strtab,
        .n_line_map = n_line_map,
        .source_map = source_map,
        .n_source_map = n_source_lines,
        .source_map_asm = source_map_asm,
        .n_source_map_asm = n_source_lines_asm,
    };

    while(1) {
        printk("[PI-DEBUG] RPi Debugger - pc=%x: %x\n", debug_info.target_regs.regs[15], *(uint32_t *)(debug_info.target_regs.regs[15]));
        printk("> ");
        
        char command[MAX_COMMAND_LENGTH];
        uint32_t len = get_command(command);

        uint32_t ret = dispatch_command(command, &source_ctx);
        if(ret == -1) {
            printk("[PI-DEBUG] Error dispatching command\n");
        }
    }
}