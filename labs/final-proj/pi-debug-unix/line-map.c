#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "libunix.h"
#include "line_map.h"

// String table to hold all unique strings
static char *string_table = NULL;
static size_t string_table_size = 0;
static size_t string_table_capacity = 0;

// Line info struct
typedef struct {
    uint32_t filename_idx;  // Index into string table
    uint32_t funcname_idx;  // Index into string table 
    uint32_t start_addr;
    uint32_t end_addr;
    uint32_t line_number;
} line_info_t;

static line_info_t *lines = NULL;
static size_t num_lines = 0;
static size_t lines_capacity = 0;

// Add string to string table, return index
size_t add_string(const char *str) {
    if (!str) return 0;
    
    // Check if string already exists in table
    char *ptr = string_table;
    size_t idx = 0;
    while (idx < string_table_size) {
        if (strcmp(ptr, str) == 0) {
            return idx;
        }
        idx += strlen(ptr) + 1;
        ptr = string_table + idx;
    }

    // Add new string
    size_t len = strlen(str) + 1;
    if (string_table_size + len > string_table_capacity) {
        string_table_capacity = string_table_capacity ? string_table_capacity * 2 : 4096;
        string_table = realloc(string_table, string_table_capacity);
    }
    
    memcpy(string_table + string_table_size, str, len);
    idx = string_table_size;
    string_table_size += len;
    return idx;
}

// Add a new line entry
void add_line(const char *filename, const char *funcname, 
                uint32_t start_addr, uint32_t end_addr, uint32_t line_num) {
    if (num_lines == lines_capacity) {
        lines_capacity = lines_capacity ? lines_capacity * 2 : 1024;
        lines = realloc(lines, lines_capacity * sizeof(line_info_t));
    }

    lines[num_lines].filename_idx = add_string(filename);
    lines[num_lines].funcname_idx = add_string(funcname);
    lines[num_lines].start_addr = start_addr;
    lines[num_lines].end_addr = end_addr;
    lines[num_lines].line_number = line_num;
    num_lines++;
}

void parse_line_map(const char *line_map_file) {
    if (lines) {
        panic("Line map already parsed\n");
    }

    FILE *fp = fopen(line_map_file, "r");
    if (!fp) {
        panic("Could not open line map file: %s\n", line_map_file);
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char filename[512];
        char funcname[128];
        unsigned line_num;
        unsigned start_addr, end_addr;
        
        // Parse line of format: filename line_num funcname start_addr end_addr
        char *p = line;
        
        // Get filename
        while (*p && isspace(*p)) p++;
        char *start = p;
        while (*p && !isspace(*p)) p++;
        if (!*p) continue;
        memcpy(filename, start, p - start);
        filename[p - start] = '\0';

        // Get line number
        while (*p && isspace(*p)) p++;
        if (!*p || !isdigit(*p)) {
            line_num = 0; // No line number
        } else {
            line_num = strtoul(p, &p, 10);
        }

        // Get function name
        while (*p && isspace(*p)) p++;
        start = p;
        while (*p && !isspace(*p)) p++;
        if (!*p) continue;
        memcpy(funcname, start, p - start);
        funcname[p - start] = '\0';

        // Get addresses
        while (*p && isspace(*p)) p++;
        if (!*p) continue;
        start_addr = strtoul(p, &p, 16);

        while (*p && isspace(*p)) p++;
        if (!*p) continue;
        end_addr = strtoul(p, NULL, 16);

        add_line(filename, funcname, start_addr, end_addr, line_num);
    }

    fclose(fp);
}

uint32_t get_string_table_size() {
    return string_table_size;
}

char* get_string_table() {
    return string_table;
}

uint32_t get_num_lines() {
    return num_lines;
}

line_info_t* get_line_info() {
    return lines;
}