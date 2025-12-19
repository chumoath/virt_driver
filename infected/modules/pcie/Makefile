ifneq ($(KERNELRELEASE),)
	obj-m = pcie_drv.o
	pcie_drv-objs := pcie_main.o pcie_cdev.o pcie_mm.o pcie_swap.o
	# ccflags-y += -DDEBUG
	EXTRA_CFLAGS += -g -fverbose-asm
else
	MODULE_PATH := $(shell pwd)
	KERNEL_PATH := '/root/linux-5.10.240/build.arm64'
	ARCH := arm64
	CROSS_COMPILE := aarch64-linux-gnu-
all:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNEL_PATH} M=${MODULE_PATH} modules 
clean:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNEL_PATH} M=${MODULE_PATH} clean
endif