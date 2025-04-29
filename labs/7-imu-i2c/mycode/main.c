#include "rpi.h"
#include "myi2c.h"
#include "mpu-driver.h"

void notmain() {
    delay_ms(100);
    my_i2c_init();
    delay_ms(100);

    mpu_init();
    mpu_accel_init(ACCEL_SCALE_2G);
    mpu_gyro_init(GYRO_SCALE_250);

    uint32_t whoami = mpu_whoami();
    output("MPU WHOAMI = 0x%x\n", whoami);

    for(int i = 0; i < 10; i++) {
        while(!mpu_intr_status()) {}

        accel_rd_t accel = mpu_read_accel();
        output("accel: x=%d, y=%d, z=%d\n", accel.x, accel.y, accel.z);

        gyro_rd_t gyro = mpu_read_gyro();
        output("gyro: x=%d, y=%d, z=%d\n", gyro.x, gyro.y, gyro.z);

        delay_ms(1000);
    }
}