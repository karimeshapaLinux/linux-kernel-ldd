PWD	:=$(shell pwd)
KERNELDIR	:= /lib/modules/$(shell uname -r)/build

obj-m += ramdisk_blk.o

# -C $KDIR
# The directory where the kernel source is located.

# M=$PWD
# Informs kbuild that an external module is being built.

# modules : Default target for make command

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers bd* private*

help:
	echo "Hello, ramdisk_blk v0.0"
