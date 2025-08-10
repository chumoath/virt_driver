#!/bin/bash

ps -ef | grep qemu | grep -v grep | awk '{print $2}' | xargs kill -9
sleep 1
rm -f /tmp/vm-socket

ip tuntap add tap0 mode tap group 0
ip link set dev tap0 up
ip addr add 192.168.33.1/24 dev tap0
iptables -A POSTROUTING -t nat -j MASQUERADE -s 192.168.33.0/24
echo 1 > /proc/sys/net/ipv4/ip_forward
iptables -P FORWARD ACCEPT

cur_path=$(cd "$(dirname "$0")"; pwd)
bin_path=$cur_path/../../bin/

cd $bin_path
./qemu-system-aarch64 -M virt,gic-version=3 \
    -nographic \
    -m 1024M \
    -cpu cortex-a72 \
    -smp 8 \
    -kernel $bin_path/Image \
    -dtb $bin_path/arm64.dtb \
    -drive format=raw,file=$bin_path/vm1.ext4 \
    -append "console=ttyAMA0 root=/dev/vda rw nokaslr" \
    -net nic,netdev=tap0,model=virtio -netdev tap,id=tap0,ifname=tap0,script=no,downscript=no  \
    -serial telnet::55555,server,nowait,nodelay \
    -serial unix:/tmp/vm-socket,server,nowait \
    -monitor none \
    -s &

cd -

sleep 3
telnet localhost 55555
