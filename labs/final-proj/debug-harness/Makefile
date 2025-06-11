PROGS := debug-harness.c

# INC += -I./includes
COMMON_SRC += strlib.c
COMMON_SRC += commands.c
COMMON_SRC += breakpoints.c

INC += -I./fat32/

CFLAGS += -mtp=soft

LPI_STAFF_OBJS = $(CS240LX_2025_PATH)/libpi/staff-objs/
STATIC_DEPS = ./static-deps
# STAFF_OBJS += $(LPI_STAFF_OBJS)/kmalloc.o
STAFF_OBJS += $(LPI_STAFF_OBJS)/interrupts-vec-asm.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/staff-pinned-vm.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/staff-mmu-asm.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/staff-mmu.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/staff-armv6-except.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/staff-breakpoint.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/map-user-to-staff-fn.o
# STAFF_OBJS += $(LPI_STAFF_OBJS)/staff-watchpoint.o

# For filesystem
STAFF_OBJS +=  ./fat32/staff-mbr.o

# uncomment COMMON_SRC and comment out STAFF_OBJS
# COMMON_SRC += fat32.c
STAFF_OBJS +=  ./fat32/staff-fat32.o

CFLAGS_EXTRA  = -I./fat32/external-code

# a list of all of your object files.
COMMON_SRC += ./fat32/pi-sd.c ./fat32/mbr-helpers.c ./fat32/fat32-helpers.c ./fat32/fat32-lfn-helpers.c ./fat32/external-code/unicode-utf8.c ./fat32/external-code/emmc.c#  external-code/mbox.c 


RUN = 0

include $(CS240LX_2025_PATH)/libpi/mk/Makefile.robust-v2
