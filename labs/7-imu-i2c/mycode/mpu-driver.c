#include "rpi.h"
#include "myi2c.h"
#include "mpu-driver.h"
#include "gpio.h"

void mpu_init() {
    // Tell MPU to reset
    // uint8_t mpu_power = mpu_read(POWER_MGMT_1);
    // trace("MPU power 0x%x\n", mpu_power);
    mpu_write(POWER_MGMT_1, POWER_MGMT_1_RESET);
    delay_ms(100);

    // Zero out register -- turns off sleep mode, doesn't affect anything else
    mpu_write(POWER_MGMT_1, 0x00);
    delay_ms(100);

    // Interrupts latch (keep high)
    mpu_write(INT_CONFIG_REG, LATCH_INT_EN | INT_ANYRD_2CLEAR);

    // Raw data ready interrupt enabled
    mpu_write(INT_EN_REG, RAW_RDY_EN);

    // Configure GPIO pin to read INTR
    dev_barrier();
    gpio_set_function(GPIO_INTR_PIN, GPIO_FUNC_INPUT);
    dev_barrier();

    trace("MPU initialized\n");
}

void mpu_accel_init(accel_scale_t scale) {
    // Set accelerometer scale value
    uint8_t accel_config = mpu_read(ACCEL_CONFIG);
    accel_config &= ~(0b11 << 3);
    accel_config |= ((scale & 0b11) << 3);
    mpu_write(ACCEL_CONFIG, accel_config);
}

void mpu_gyro_init(gyro_scale_t scale) {
    // Set gyroscope scale value
    uint8_t gyro_config = mpu_read(GYRO_CONFIG);
    gyro_config &= ~(0b11 << 3);
    gyro_config |= ((scale & 0b11) << 3);
    mpu_write(GYRO_CONFIG, gyro_config);
}

uint8_t mpu_read(uint8_t reg) {
    my_i2c_write_nbytes(1, MPU_ADDR, &reg);
    
    uint8_t result;
    my_i2c_read_nbytes(1, MPU_ADDR, &result);
    
    return result;
}

void mpu_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    my_i2c_write_nbytes(2, MPU_ADDR, data);
}

uint8_t mpu_whoami() {
    return mpu_read(WHOAMI);
}

uint8_t mpu_intr_status() {
    return gpio_read(GPIO_INTR_PIN);
}

accel_rd_t mpu_read_accel() {
    int16_t x = (int16_t) (mpu_read(ACCEL_XOUT_H) << 8 | mpu_read(ACCEL_XOUT_L));
    int16_t y = (int16_t) (mpu_read(ACCEL_YOUT_H) << 8 | mpu_read(ACCEL_YOUT_L));
    int16_t z = (int16_t) (mpu_read(ACCEL_ZOUT_H) << 8 | mpu_read(ACCEL_ZOUT_L));

    return (accel_rd_t) { .x = mpu_convert_accel(x), .y = mpu_convert_accel(y), .z = mpu_convert_accel(z) };
}

gyro_rd_t mpu_read_gyro() {
    int16_t x = (int16_t) (mpu_read(GYRO_XOUT_H) << 8 | mpu_read(GYRO_XOUT_L));
    int16_t y = (int16_t) (mpu_read(GYRO_YOUT_H) << 8 | mpu_read(GYRO_YOUT_L));
    int16_t z = (int16_t) (mpu_read(GYRO_ZOUT_H) << 8 | mpu_read(GYRO_ZOUT_L));

    return (gyro_rd_t) { .x = mpu_convert_gyro(x), .y = mpu_convert_gyro(y), .z = mpu_convert_gyro(z) };
}

int16_t mpu_convert_accel(int16_t val) {
    // Takes (value * 1000 * total range of g) / total range of measurement

    // ACCEL_RANGE_2G = 0 and has range +-2g -> 4g, so
    // Compute total range of g's (2 * 2^(CONST + 1) = 2^(CONST + 2) = 1 << (CONST + 2)) 
    int g_range = (int) (1 << (ACCEL_SCALE_2G + 2));
    int measurement_range = (int) (0xFFFF + 1); // 16 bit number

    return (val * 1000 * g_range) / measurement_range;
}

int16_t mpu_convert_gyro(int16_t val) {
     // Takes (value * total range of dps) / total range of measurement

    // GYRO_RANGE_250 = 0 and has range +-250 -> 500, so
    // Compute total range of dpss (2 * 250 * 2^(CONST) = 250 * 2^(CONST + 1) = 250 * 1 << (CONST + 1)) 
    // int dps_range = 250 * (1 << (h->dps + 1));
    // int measurement_range = (int) (0xFFFF + 1); // 16 bit number

    double multiplier = 0;
    switch (GYRO_SCALE_250) {
        case GYRO_SCALE_250:
            multiplier = 131;
            break;
        case GYRO_SCALE_500:
            multiplier = 65.5;
            break;
        case GYRO_SCALE_1000:
            multiplier = 32.8;
            break;
        case GYRO_SCALE_2000:
            multiplier = 16.4;
            break;
    }

    return (int16_t) (val * 1000) / multiplier;
}