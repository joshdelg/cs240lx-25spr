#include "rpi.h"
#include "myi2c.h"
#include "mpu-driver.h"
#include "gpio.h"
#include "rpi-math.h"
#include "bit-support.h"

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
    return val;
}

accel_rd_t mpu_get_self_test_accel(void) {
    uint16_t x = (mpu_read(SELF_TEST_13) & 0b11100000) >> 3;
    x |= (mpu_read(SELF_TEST_16) & 0b00110000) >> 4;

    uint16_t y = (mpu_read(SELF_TEST_14) & 0b11100000) >> 3;
    y |= (mpu_read(SELF_TEST_16) & 0b00001100) >> 2;

    uint16_t z = (mpu_read(SELF_TEST_15) & 0b11100000) >> 3;
    z |= (mpu_read(SELF_TEST_16) & 0b00000011);

    return (accel_rd_t) { .x = x, .y = y, .z = z };
}

gyro_rd_t mpu_get_self_test_gyro(void) {
    uint16_t x = (mpu_read(SELF_TEST_13) & 0b00011111);

    uint16_t y = (mpu_read(SELF_TEST_14) & 0b00011111);

    uint16_t z = (mpu_read(SELF_TEST_15) & 0b00011111);

    return (gyro_rd_t) { .x = x, .y = y, .z = z };
}

void mpu_gryo_init_self_test(void) {

}

uint16_t mpu_gyro_self_test(void) {
    // Read without self test
    gyro_rd_t init = mpu_read_gyro();

    // Enable self test
    uint8_t val = mpu_read(GYRO_CONFIG);
    val = bit_set(val, 7);
    val = bit_set(val, 6);
    val = bit_set(val, 5);

    mpu_write(GYRO_CONFIG, val);

    delay_ms(250);

    for(int i = 0; i < 20; i++) {
        while(!mpu_intr_status()) {}

        mpu_read_gyro(); // Ignore
    }

    gyro_rd_t vals = mpu_read_gyro();

    gyro_rd_t ft = mpu_get_self_test_gyro();

    float str_x = (float)init.x - (float)vals.x;
    float str_y = (float)init.y - (float)vals.y;
    float str_z = (float)init.z - (float)vals.z;

    output("str_x = %f\n", str_x);
    output("str_y = %f\n", str_y);
    output("str_z = %f\n", str_z);

    float ft_z =  25. * 131. * powf(1.046, ft.z - 1.);
    float ft_x =  25. * 131. * powf(1.046, ft.x - 1.);
    float ft_y = -25. * 131. * powf(1.046, ft.y - 1.);

    output("ft_x = %f\n", ft_x);
    output("ft_y = %f\n", ft_y);
    output("ft_z = %f\n", ft_z);

    float diff_x = 100 + ((str_x - ft_x) / (ft_x + str_x)) * 100;
    float diff_y = 100 + ((str_y - ft_y) / (ft_y + str_y)) * 100;
    float diff_z = 100 + ((str_z - ft_z) / (ft_z + str_z)) * 100;

    output("diff_x = %f\n", diff_x);
    output("diff_y = %f\n", diff_y);
    output("diff_z = %f\n", diff_z);

    return 1;
}