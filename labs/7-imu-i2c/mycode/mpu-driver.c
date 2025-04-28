#include "rpi.h"
#include "myi2c.h"
#include "mpu-driver.h"

void mpu_init() {
    // Tell MPU to reset
    // uint8_t mpu_power = mpu_read(POWER_MGMT_1);
    // trace("MPU power 0x%x\n", mpu_power);
    mpu_write(POWER_MGMT_1, POWER_MGMT_1_RESET);
    delay_ms(100);

    // Zero out register -- turns off sleep mode, doesn't affect anything else
    mpu_write(POWER_MGMT_1, 0x00);
    delay_ms(100);

    trace("MPU initialized\n");
}

uint8_t mpu_read(uint8_t reg) {
    my_i2c_write_nbytes(1, MPU_ADDR, &reg);
    
    uint8_t result;
    my_i2c_read_nbytes(1, MPU_ADDR, &result);

    trace("Read 0x%x\n", result);
    
    return result;
}

void mpu_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    my_i2c_write_nbytes(2, MPU_ADDR, data);
}

uint8_t mpu_whoami() {
    return mpu_read(WHOAMI);
}