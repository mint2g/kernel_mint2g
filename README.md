Stock kernel (SEA) for Samsung Galaxy Star Duos from samsung opensource.
GT-S5282_SEA_JB_Opensource.zip

Contents of Readme_KERNEL.txt found inside the zip.
################################################################################

1. How to Build
	- get Toolchain
		From android git server , codesourcery and etc ..
		 - arm-eabi-4.4.3
		
	- edit Makefile
		edit target architecture.
		 - ARCH ?= arm
		edit "CROSS_COMPILE" to right toolchain path(You downloaded).
		  EX)  CROSS_COMPILE= $(android platform directory you download)/android/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-
          Ex)  CROSS_COMPILE=/usr/local/toolchain/arm-eabi-4.4.3/bin/arm-eabi-          // check the location of toolchain
  	
		$ make mint-vlx-rev03_defconfig
		$ make zImage

2. Output files
	- Kernel : arch/arm/boot/zImage

3. How to Clean	
		$ make clean
################################################################################
