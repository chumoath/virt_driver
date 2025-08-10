#!/bin/bash

cur_path=$(cd "$(dirname "$0")"; pwd)
script_path=$cur_path/../
uartnet_path=$script_path/../
guest_path=$script_path/guest/
bin_path=$uartnet_path/bin/

# uart_net/bin
rm -rf $bin_path
tar -xf $script_path/bin.tar.gz -C $uartnet_path

tar -xf $bin_path/core-image-full-cmdline-qemuarm64.rootfs.ext4.tar.gz -C $bin_path

# vm1.ext4
cp -fa $bin_path/core-image-full-cmdline-qemuarm64.rootfs.ext4 $bin_path/vm1.ext4
mount $bin_path/vm1.ext4 /mnt
cp -fa $guest_path/config_vm1.sh /mnt/home/root
chmod +x /mnt/home/root/config_vm1.sh
cp -fa $guest_path/vm1_interfaces /mnt/etc/network/interfaces
sync
umount /mnt


# vm2.ext4
cp -fa $bin_path/core-image-full-cmdline-qemuarm64.rootfs.ext4 $bin_path/vm2.ext4
mount $bin_path/vm2.ext4 /mnt
cp -fa $guest_path/config_vm2.sh /mnt/home/root
chmod +x /mnt/home/root/config_vm2.sh
cp -fa $guest_path/vm2_interfaces /mnt/etc/network/interfaces
sync
umount /mnt
