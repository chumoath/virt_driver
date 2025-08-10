#!/bin/bash

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

cur_path=$(cd "$(dirname "$0")"; pwd)
bin_path=$cur_path/../../bin/
#make
scp $bin_path/uart_ethernet.ko 192.168.33.2:/
scp $bin_path/uart_ethernet.ko 192.168.0.2:/
