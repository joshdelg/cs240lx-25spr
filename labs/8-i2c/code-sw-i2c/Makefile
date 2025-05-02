# double check that everything works and then swap 
# these so it uses your sw-i2c.c
COMMON_SRC = sw-i2c.c 
# STAFF_OBJS += staff-sw-i2c.o 

# a vaneer to let the big banged work with code
# that assumes the hardware interface.
# COMMON_SRC += sw-i2c-vaneer.c
COMMON_SRC += my-sw-i2c-vaneer.c

# use last lab's code
IMU_DIR := ../../7-imu-i2c/code/
# PROGS = $(IMU_DIR)/driver-gyro.c

# PROGS = $(IMU_DIR)/driver-accel.c

MY_IMU_DIR := ../../7-imu-i2c/mycode/
PROGS = $(MY_IMU_DIR)/main.c

# when done, should use yours.
# pick either yours or ours.
# COMMON_SRC += $(IMU_DIR)/mpu-6050.c
COMMON_SRC += $(MY_IMU_DIR)/mpu-driver.c
# STAFF_OBJS += $(IMU_DIR)/staff-mpu-6050.o

# define this if you need to give the device for your pi
TTYUSB = 
BOOTLOADER = my-install

# set to 0 or comment out if you don't want it to run automatically.
RUN=1

LIBS += $(LGCC)
LIB_POST := $(LGCC)

LIBS += $(LIBM)
CFLAGS += $(LIBM_INC)

include ../../../libpi/mk/Makefile.robust-v2
