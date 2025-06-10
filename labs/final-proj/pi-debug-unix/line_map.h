#ifndef LINE_MAP_H
#define LINE_MAP_H

#include <stdint.h>

typedef struct {
    uint32_t filename_idx;
    uint32_t funcname_idx;
    uint32_t start_addr;
    uint32_t end_addr;
    uint32_t line_number;
} line_info_t;

// Parse the line map file and store the line information in an array of line_info_t structs
void parse_line_map(const char *line_map_file);

// // Transmit the string table to the RPI
// void transmit_string_table();

// // Transmit the line information to the RPI
// void transmit_line_info();

uint32_t get_string_table_size();
char* get_string_table();

line_info_t* get_line_info();
uint32_t get_num_lines();

#endif