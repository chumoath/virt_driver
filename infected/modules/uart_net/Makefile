ifneq ($(KERNELRELEASE),)
	obj-m = uart_ethernet.o
	uart_ethernet-y += uart_net.o
	ccflags-y += -DUART_DEBUG_ENABLE
else
	PWD := $(shell pwd)
all:
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /root/linux-5.10.240 M=${PWD} modules 
clean:
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /root/linux-5.10.240 M=${PWD} clean
endif
