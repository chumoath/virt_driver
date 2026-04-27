ifneq ($(KERNELRELEASE),)
    obj-m := test_notifier.o
else
    MODULE_PATH := $(shell pwd)
    ARCH := arm64
    KERNEL_PATH := '/root/linux-5.10.248/build.arm64'
    CROSS_COMPILE := aarch64-linux-gnu-

#edu-cli: edu-cli.c
#	$(CC) -Wall -Wextra -std=gnu11 -o $@ $<

all:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNEL_PATH} M=${MODULE_PATH} modules

clean:
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C ${KERNEL_PATH} M=${MODULE_PATH} clean
endif
