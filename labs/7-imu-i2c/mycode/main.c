#include "rpi.h"
#include "myi2c.h"
#include "mpu-driver.h"

void notmain() {
    output("Starting!\n");
    delay_ms(100);
    output("Calling i2c init\n");
    my_i2c_init();
    delay_ms(100);

    output("Calling mpu init\n");
    mpu_init();
    output("Finished mpu init\n");
    mpu_accel_init(ACCEL_SCALE_8G);
    mpu_gyro_init(GYRO_SCALE_250);

    uint32_t whoami = mpu_whoami();
    output("MPU WHOAMI = 0x%x\n", whoami);

    delay_ms(250);

    // Run self test
    mpu_gyro_self_test();

    mpu_accel_self_test();



    for(int i = 0; i < 10; i++) {
        while(!mpu_intr_status()) {}

        accel_rd_t accel = mpu_read_accel();
        output("accel: x=%d, y=%d, z=%d\n", accel.x, accel.y, accel.z);

        gyro_rd_t gyro = mpu_read_gyro();
        output("gyro: x=%d, y=%d, z=%d\n", gyro.x, gyro.y, gyro.z);

        delay_ms(1000);
    }
}