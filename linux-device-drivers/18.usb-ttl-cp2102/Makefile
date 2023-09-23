obj-m += serial_cp2102.o
serial_cp2102-objs := usb_ttl_cp2102.o cp2102if.o

PWD	:=$(shell pwd)
KERNELDIR	:= /home/karimeshapa/ldd-ws/rpi2-b-bcm2836r/linux
CROSSCOMPILE=arm-linux-gnueabihf-
ARCH=arm

# -C $KDIR
# The directory where the kernel source is located.

# M=$PWD
# Informs kbuild that an external module is being built.

# modules : Default target for make command

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSSCOMPILE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers

help:
	echo "Hello, serial_cp2102 v0.0"
