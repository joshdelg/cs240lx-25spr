#include "rpi.h"

void notmain(void) {
    printk("Starting debugger!\n");

    // @joshdelg I don't know why, but I need to flush UART before we branch
    // I imagine this is becuase UART is initialized in debug harness, and
    // then again in the debug target ??
    uart_flush_tx();

    // Branch to entrypoint
    BRANCHTO(0x50000);
}