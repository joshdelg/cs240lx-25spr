#include "rpi.h"
#include "myi2c.h"
#include "mpu-driver.h"

void notmain() {
    delay_ms(100);
    my_i2c_init();
    delay_ms(100);

    uint32_t i2c_control = GET32(I2C_CTRL);
    output("I2C_CTRL = 0x%x\n", i2c_control);
    output("I12C Clock Divisor = 0x%x\n", GET32(I2C_DIV));

    // my_i2c_set_secondary_addr(MPU_ADDR);
    // my_i2c_set_clock_div(MPU_CLOCK_DIV);
    // my_i2c_set_clock_div(CLOCK_DIV_RESET);

    mpu_init();

    uint32_t whoami = mpu_whoami();
    output("MPU WHOAMI = 0x%x\n", whoami);
}