Create PCI device using Qemu emulation
======================================
As of now I don't have boards support pci, so let's emulate the device, we need to build Qemu and linux image for x86.
Most of this document is based on the following article, just document all things,
https://gist.github.com/ncmiller/d61348b27cb17debd2a6c20966409e86
-------------------------------------------------------------------
- Install requirements packages
	sudo apt-get install libaio-dev libbluetooth-dev libcapstone-dev libbrlapi-dev libbz2-dev
	sudo apt-get install libcap-ng-dev libcurl4-gnutls-dev libgtk-3-dev
	sudo apt-get install libibverbs-dev libjpeg8-dev libncurses5-dev libnuma-dev
	sudo apt-get install librbd-dev librdmacm-dev
	sudo apt-get install libsasl2-dev libsdl2-dev libseccomp-dev libsnappy-dev libssh-dev
	sudo apt-get install libvde-dev libvdeplug-dev libvte-2.91-dev libxen-dev liblzo2-dev
	sudo apt-get install valgrind xfslibs-dev 
- Prepare a working space for kernel development
	cd $HOME
	mkdir kdev
	TOP=$HOME/kdev
- Clone busyBox source code for constructing intrd.
	git clone git://busybox.net/busybox.git --branch=1_33_0 --depth=1
- Clone linux kernel
	git clone https://github.com/torvalds/linux.git
- Configure busybox 
	cd busyBox
	mkdir -p home/build/busybox-x86
	make O=$TOP/build/busybox-x86 defconfig
	make O=$TOP/build/busybox-x86 menuconfig
- Enable building BusyBox as a static binary
	-> Busybox Settings
  		-> Build Options
	[*] Build BusyBox as a static binary (no shared libs)
- Build and install busybox
	cd $TOP/build/busybox-x86
	make -j4
	make install
- Create minimal filesystem
	mkdir -p $TOP/build/initramfs/busybox-x86
	cd $TOP/build/initramfs/busybox-x86
	mkdir -pv {bin,sbin,etc,proc,sys,usr/{bin,sbin}}
	cp -av $TOP/build/busybox-x86/_install/* .
- Create a file called init with the following contents
	#!/bin/sh
 
	mount -t proc none /proc
	mount -t sysfs none /sys
 	echo -e "\nBoot took $(cut -d' ' -f1 /proc/uptime) seconds\n"
 	exec /bin/sh

- Make it executable
	chmod +x init
- Generate initramfs
	find . -print0 | cpio --null -ov --format=newc | gzip -9 > $TOP/build/initramfs-busybox-x86.cpio.gz
- Configure Linux using simple defconfig and kvm options
	cd $TOP/linux
	make O=$TOP/build/linux-x86-basic x86_64_defconfig
	make O=$TOP/build/linux-x86-basic kvmconfig
- Build Linux
	make O=$TOP/build/linux-x86-basic -j4

- Clone Qemu
	git clone https://gitlab.com/qemu-project/qemu.git
- Add our pci device code to be emulated
	copy pci_dev_emu.c inside qemu/hw/pci/ directory
- Add emulated device to build system
	 open qemu/hw/pci/meson.build
	 add our device file name at the end of this list,
	
	pci_ss = ss.source_set()
	pci_ss.add(files(
	  'msi.c',
	  'msix.c',
	  'pci.c',
	  'pci_bridge.c',
	  'pci_host.c',
	  'pci-hmp-cmds.c',
	  'pci-qmp-cmds.c',
	  'pcie_sriov.c',
	  'shpc.c',
	  'slotid_cap.c',
	  'pci_dev_emu.c'
	))
	
- Build Qemu with our changes
	cd qemu
	git submodule init
	git submodule update --recursive
	./configure --target-list=x86_64-softmmu --enable-debug
	make -j4
- Run Qemu with your built kerenl, built initrd and adding our pci device 
	qemu/build/qemu-system-x86_64 -kernel kernel/linux/arch/x86_64/boot/bzImage -initrd build/build/initramfs-busybox-x86.cpio.gz -device pci-dev-emu,bus=pci.0 -nographic -append "console=ttyS0" -enable-kvm	






