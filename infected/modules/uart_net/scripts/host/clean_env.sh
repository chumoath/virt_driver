#!/bin/bash

cur_path=$(cd "$(dirname "$0")"; pwd)
script_path=$cur_path/../
uartnet_path=$script_path/../
bin_path=$uartnet_path/bin/

ps -ef | grep qemu | grep -v grep | awk '{print $2}' | xargs kill -9
sleep 1
rm -f /tmp/vm-socket
rm -rf $bin_path
