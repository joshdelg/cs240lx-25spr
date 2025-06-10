// <rpi.h> has definitions for <putk> and <clean_reboot>.
// <libpi.a> has their implementations.
#include "rpi.h"

void notmain(void) {
    printk("hello world from the pi\n");
    printk("hello world 2\n");
    printk("hello world 3\n");
    printk("hello world 4\n");
    printk("hello world 5\n");
    printk("hello world 6\n");
    printk("hello world 7\n");
    printk("hello world 8\n");
    printk("hello world 9\n");
    printk("hello world 10\n");
}
