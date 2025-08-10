ifneq ($(KERNELRELEASE),)
	obj-m = uart_ethernet.o
	uart_ethernet-y += uart_net.o
	ccflags-y += -DUART_DEBUG_ENABLE
else
all:
	make -C /root/linux-5.10.240 M="/root/linux-5.10.240/uart_net" modules 
clean:
	make -C /root/linux-5.10.240 M="/root/linux-5.10.240/uart_net" clean
endif
