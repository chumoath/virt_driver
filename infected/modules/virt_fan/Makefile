ifneq ($(KERNELRELEASE),)
	# KBUILD_EXTRA_SYMBOLS += ${SYMVERS_DIR}/vmlinux.symvers
	obj-m := virt_fan_drv.o
	virt_fan_drv-y += virt_fan.o virt_trans.o
else
	PWD := $(shell pwd)

default:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNEL_BUILD_DIR} M=${PWD} modules

clean:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNEL_BUILD_DIR} M=${PWD} clean
endif