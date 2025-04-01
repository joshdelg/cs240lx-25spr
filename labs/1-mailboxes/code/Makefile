PROGS := test-mbox.c
COMMON_SRC := mbox.c

# define this if you need to give the device for your pi
TTYUSB = 
BOOTLOADER = my-install

# uncomment if you want it to automatically run.
RUN=1

# in case you need division.
LIB_POST += $(CS240LX_2025_PATH)/lib/libgcc.a
include $(CS240LX_2025_PATH)/libpi/mk/Makefile.robust-v2
