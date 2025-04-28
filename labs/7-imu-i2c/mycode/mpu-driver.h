#ifndef JOSH_MPU_DRIVER_H
#define JOSH_MPU_DRIVER_H

enum {
    WHOAMI = 117,
    POWER_MGMT_1 = 107,
    POWER_MGMT_2 = 108,
    POWER_MGMT_1_RESET = 1 << 7,
};

enum {
    // RPI Clock = 150 Mhx
    // MPU Clock = 400 Khz
    // RPI Clock / MPU Clock = 375 = 0x177
    MPU_CLOCK_DIV = 0x177,
    MPU_ADDR = 0b1101000
};

void mpu_init(void);

uint8_t mpu_read(uint8_t reg);
void mpu_write(uint8_t reg, uint8_t value);

uint8_t mpu_whoami(void);

#endif