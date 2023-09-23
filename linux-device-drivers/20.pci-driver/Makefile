PWD	:=$(shell pwd)

# Build with the corresponding kernel used for Qemu
KERNELDIR	:= /home/karimeshapa/ldd-ws/kern-emu/kernel/linux

obj-m += pci_drv.o

# -C $KDIR
# The directory where the kernel source is located.

# M=$PWD
# Informs kbuild that an external module is being built.

# modules : Default target for make command

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o .*.o.d *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers

help:
	echo "Hello, pci_drv v0.0"