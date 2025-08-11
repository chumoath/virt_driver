ifneq ($(KERNELRELEASE),)
	obj-m = uart_ethernet.o
	uart_ethernet-y += uart_net.o
	ccflags-y += -DUART_DEBUG_ENABLE
else
	PWD := $(shell pwd)
all:
	make -C /root/linux-5.10.240 M=${PWD} modules 
clean:
	make -C /root/linux-5.10.240 M=${PWD} clean
endif
