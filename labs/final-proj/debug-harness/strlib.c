#include "strlib.h"

char *strtok_sp(char *str) {
    // printk("strtok_sp called with str: %s\n", str);
    static char *next_start = NULL;

    if (str != NULL) {
        // First call - so use start of string
        next_start = str;
    }

    // printk("next_start: %s\n", next_start);

    // No more tokens
    if (*next_start == '\0') {
        return NULL;
    }

    char *token = next_start;

    while(*next_start == ' ') {
        next_start++;
    }

    while(*next_start != '\0' && *next_start != ' ') {
        next_start++;
    }

    // End of string
    if (*next_start != '\0') {
        // Otherwise, insert \0 to terminate this string and increment
        *next_start = '\0';
        next_start++;
    }

    return token;
}

uint32_t str_to_uint32(char *str) {
    uint32_t num = 0;
    for(int i = 0; i < strlen(str); i++) {
        if(str[i] >= '0' && str[i] <= '9') {
            num = (num << 4) | (str[i] - '0');
        } else if(str[i] >= 'a' && str[i] <= 'f') {
            num = (num << 4) | (str[i] - 'a' + 10);
        } else if(str[i] >= 'A' && str[i] <= 'F') {
            num = (num << 4) | (str[i] - 'A' + 10);
        } else {
            printk("Invalid character in string: %s\n", str);
            return 0;
        }
    }
    return num;
}