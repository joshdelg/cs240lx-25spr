#ifndef JOSH_MPU_DRIVER_H
#define JOSH_MPU_DRIVER_H

enum {
    WHOAMI = 117,
    POWER_MGMT_1 = 107,
    POWER_MGMT_2 = 108,
    POWER_MGMT_1_RESET = 1 << 7,
    GYRO_CONFIG = 27,
    ACCEL_CONFIG = 28,
    INT_CONFIG_REG = 55,
    LATCH_INT_EN = 1 << 5,
    INT_ANYRD_2CLEAR = 1 << 4,
    INT_EN_REG = 56,
    RAW_RDY_EN = 1 << 0,
    INT_STATUS_REG = 58,
    INT_CLR = 0,
    ACCEL_XOUT_H = 59,
    ACCEL_XOUT_L = 60,
    ACCEL_YOUT_H = 61,
    ACCEL_YOUT_L = 62,
    ACCEL_ZOUT_H = 63,
    ACCEL_ZOUT_L = 64,
    GYRO_XOUT_H = 67,
    GYRO_XOUT_L = 68,
    GYRO_YOUT_H = 69,
    GYRO_YOUT_L = 70,
    GYRO_ZOUT_H = 71,
    GYRO_ZOUT_L = 72,
    SELF_TEST_13 = 13,
    SELF_TEST_14 = 14,
    SELF_TEST_15 = 15,
    SELF_TEST_16 = 16
};

typedef enum {
    ACCEL_SCALE_2G = 0b00,
    ACCEL_SCALE_4G = 0b01,
    ACCEL_SCALE_8G = 0b10,
    ACCEL_SCALE_16G = 0b11,
} accel_scale_t;

typedef enum {
    GYRO_SCALE_250 = 0b00,
    GYRO_SCALE_500 = 0b01,
    GYRO_SCALE_1000 = 0b10,
    GYRO_SCALE_2000 = 0b11,
} gyro_scale_t;

enum {
    // RPI Clock = 150 Mhx
    // MPU Clock = 400 Khz
    // RPI Clock / MPU Clock = 375 = 0x177
    MPU_CLOCK_DIV = 0x177,
    MPU_ADDR = 0b1101000,
    GPIO_INTR_PIN = 4
};

typedef struct {
    int16_t x, y, z;
} accel_rd_t;

typedef struct {
    int16_t x, y, z;
} gyro_rd_t;

void    mpu_init(void);
void    mpu_accel_init(accel_scale_t scale);
void    mpu_gyro_init(gyro_scale_t scale);

uint8_t mpu_read(uint8_t reg);
void    mpu_write(uint8_t reg, uint8_t value);

uint8_t mpu_whoami(void);

uint8_t mpu_intr_status(void);
void    mpu_intr_clear(void);

accel_rd_t  mpu_read_accel(void);
int16_t     mpu_convert_accel(int16_t val);

gyro_rd_t   mpu_read_gyro(void);
int16_t     mpu_convert_gyro(int16_t val);

uint16_t mpu_gyro_self_test(void);
uint16_t mpu_accel_self_test(void);

#endif