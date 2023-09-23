PWD	:=$(shell pwd)
KERNELDIR	:= /lib/modules/$(shell uname -r)/build

obj-m += virt_tty.o

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
	echo "Hello, virt_tty v0.0"

