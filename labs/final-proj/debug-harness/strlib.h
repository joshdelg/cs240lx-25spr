#ifndef RPI_STRLIB_H
#define RPI_STRLIB_H

#include "rpi.h"

#define MAX_COMMAND_LENGTH 100

// Returns next token in str delimited by spaces
// Returns NULL if no more tokens
// Modifies str by inserting null terminators
char *strtok_sp(char *str);

uint32_t str_to_uint32(char *str);

uint32_t str_ends_with(char *str, char *suffix);


#endif