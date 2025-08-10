#!/bin/bash

dmesg -n 7
rmmod uart_ethernet.ko
insmod /uart_ethernet.ko
ip link set dev eth1 up
ip addr add 192.168.3.1/24 dev eth1
