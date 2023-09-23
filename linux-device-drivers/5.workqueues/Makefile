PWD	:=$(shell pwd)
KERNELDIR	:= /lib/modules/$(shell uname -r)/build

obj-m += jiq.o

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers
