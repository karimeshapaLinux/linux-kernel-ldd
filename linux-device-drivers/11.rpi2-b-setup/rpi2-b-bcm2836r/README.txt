Building and Booting linux and U-Boot for rpi2-b
================================================
This document is based on the following article with some manipulations and fixes.
https://blog.christophersmart.com/2016/10/27/building-and-booting-upstream-linux-and-u-boot-for-raspberry-pi-23-arm-boards/?fbclid=IwAR2lgH0vZFNtaBpK_lm8_XJSBrXQZu9lG-cGabwBP387ElxDMjupv2MdekA
------------------------------------------------------------------------------------------------------------------------
1. Connect UART on the RaspberryPi connections pins(02, 06, 10) GND, TX and RX with ttl-usb chip pins GND, RX and TX.
2. Partition your SDcard
- Assuming your host read ttl-usb device at /dev/sdb you need to check.
- Make partitions
	sudo umount /dev/sdb*
	sudo parted -s /dev/sdb \
	mklabel msdos \
	mkpart primary fat32 1M 30M \
	mkpart primary ext4 30M 100%
- Format the partitions
	sudo mkfs.vfat /dev/sdb1
	sudo mkfs.ext4 /dev/sdb2
- Mount the boot partition to /mnt for simplicity
	 sudo mount /dev/sdx1 /mnt
3. Install these dependanices at your host
	sudo apt -y install gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf u-boot-tools bison flex bc libssl-dev make gcc
	sudo apt install screen
4. Create working directory
	mkdir rpi2-b-bcm2836r
	cd rpi2-b-bcm2836r
-------------------------------------------------------------------------------------------------------------------------
5. U-Boot Bootloader
- Clone u-boot
	git clone https://github.com/u-boot/u-boot.git
	cd cd u-boot
	CROSS_COMPILE=arm-linux-gnueabihf- make rpi_2_defconfig
- Configure u-boot and save.
	make menuconfig ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
- Build
	make -j 12 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
- Copy u-boot image to SDacrd
	sudo cp -iv u-boot.bin /mnt/kernel.img
6. Proprietary bootloader files
- Get firmware
	 cd ..
	 git clone --depth 1 https://github.com/raspberrypi/firmware
- Copy needed files to SDcard
	 sudo cp -iv firmware/boot/{bootcode.bin,fixup.dat,start.elf} /mnt/
-------------------------------------------------------------------------------------------------------------------------
7. Creating an initramfs
- Copy the following 3 files in your current directory rpi2-b-bcm2836r.
	boot.cmd, group, passwd
- Clone initramfs 
	git clone https://github.com/csmart/custom-initramfs.git
	cd custom-initramfs
- Open and change the script create_initramfs.sh
	- line 31: urandom_url="https://github.com/maximeh/buildroot/blob/master/package/initscripts/init.d/S20urandom"
	- line 152: pkgurl=http://archives.fedoraproject.org/pub/archive/fedora/linux/releases/33/Everything/armhfp/os/Packages/b/${pkg}
	- Comment line 262
	- Add a new line with the following
  	  cp ../passwd ${DIR}/initramfs/etc/
	- Comment line 265
	- Add a new line with the following
	  cp ../group ${DIR}/initramfs/etc/
	- Comment line 292
- Run script
	 ./create_initramfs.sh --arch arm --dir "${PWD}" --tty ttyAMA0
- Convert initramfs and copy to SDcard
	gunzip initramfs-arm.cpio.gz
	sudo mkimage -A arm -T ramdisk -C none -n uInitrd \
	-d initramfs-arm.cpio /mnt/uInitrd
-------------------------------------------------------------------------------------------------------------------------
8. Linux Kernel
- Clone and build
	git clone --depth=1 https://github.com/raspberrypi/linux
	cd linux
	ARCH=arm CROSS_COMPILE=arm-linux-gnu- make bcm2835_defconfig
- Configure kernel and save.
	 make menuconfig ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
- Build
	make -j12 zImage modules dtbs ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
- Copy to SDcard
	sudo cp -iv arch/arm/boot/zImage /mnt/
	sudo cp -iv arch/arm/boot/dts/bcm2836-rpi-2-b.dtb /mnt/
-------------------------------------------------------------------------------------------------------------------------
9. Bootloader config
- Compile boot.cmd file and send to SDcard
	 sudo mkimage -C none -A arm -T script -d boot.cmd /mnt/boot.scr
-------------------------------------------------------------------------------------------------------------------------
10. unmount SDcard
-  unmount and then remove SDcard and test
	sync && sudo umount /mnt
	sudo umount /dev/sdb*
-------------------------------------------------------------------------------------------------------------------------
11. Connect to rpi2-b
- Host to rpi2-b communication using screen
	sudo gpasswd -a ${USER} dialout
	newgrp dialout
	screen /dev/ttyUSB0 115200
- arm login : root
- no password
	
	 	
	
		
		 
	 
	
	
	

