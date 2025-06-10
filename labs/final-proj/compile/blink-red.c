#include "rpi.h"
#include "gpio.h"

void notmain(void) {
    gpio_set_output(21);
    printk("Turning GPIO 21 on\n");
    gpio_set_on(21);
    delay_ms(1000);
    printk("Turning GPIO 21 off\n");
    gpio_set_off(21);
    delay_ms(1000);
}