qemu-img create -f qcow2 swap.qcow2 32G
qemu-img convert -f raw -O qcow2 swap.img swap.qcow2
qemu-img convert -f raw -O qcow2 ubuntu22_arm64.img ubuntu22_arm64.qcow2
telnet localhost 55555

ip tuntap add tap0 mode tap group 0
ip link set dev tap0 up
ip addr add 192.168.33.1/24 dev tap0
iptables -A POSTROUTING -t nat -j MASQUERADE -s 192.168.33.0/24
echo 1 > /proc/sys/net/ipv4/ip_forward
iptables -P FORWARD ACCEPT

dmesg -n 7
mkswap /dev/vdb
swapon /dev/vdb

savevm swap
-loadvm swap

dd if=/dev/urandom of=test.img bs=1G count=2
scp ./pcie_drv.ko 192.168.33.2:/root/

# 内存必须 >= 23GB，arm_virt内存地址空间 0x40000000-0x4010000000；预留内存从0x400000000(16GB)开始，预留8GB，即内存地址空间必须到0x600000000(24GB)；
#                  arm_virt从0x40000000(1GB)开始，还需要23GB；
# 否则PCIe处理化物理内存描述符会失败
qemu-system-aarch64 -M virt,gic-version=3 -m 23G -cpu cortex-a72 -smp 4 -kernel Image -dtb arm64.dtb  \
                    -append "console=ttyAMA0 nokaslr root=/dev/vda rw iomem=relaxed" \
                    -device e1000e,netdev=tap0 -netdev tap,id=tap0,ifname=tap0,script=no,downscript=no \
                    -drive format=qcow2,file=ubuntu22_arm64.qcow2 -drive format=qcow2,file=swap.qcow2 \
                    -serial stdio -monitor none -s

MAX_FDT_SIZE # 改为 SZ_4M, 即使 arm64.dtb为1M也不行, 尽量小; 否则一直卡住.

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-  O=build.arm64 Image -j24
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-  O=build.arm64 modules -j24

dtc -I dtb -O dts -o arm64.dts arm64.dtb
dtc -I dts -O dtb -o arm64.dtb arm64.dts

reserved-memory {
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;
    pciebase_region@50000000 {
        reg = <0x0 0x50000000 0x0 0x40000000>;
        no-map;
    };
};

# 8G: 不添加 no-map，系统会自动映射到虚拟内存，但是/proc/iomem不会显示保留内存，且free -h不会包括此处预留的内存; 不能用devmem2访问？加了iomem=relaxed也不行，为什么？
#     添加 no-map，/proc/iomem会显示保留内存，且free -h不会包括此处预留的内存；可以用devmem2访问
reserved-memory {
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;
    pciebase_region@400000000 {
        reg = <0x4 0x0 0x2 0x0>;
    };
};

addr2line -e  build.arm64/vmlinux FFFF8000104506F0

# 2G - 4K
dd if=test.img of=xx bs=4K count=524287

sed -i 's@-fno-allow-store-data-races@@g' compile_commands.json
sed -i 's@-femit-struct-debug-baseonly@@g' compile_commands.json
sed -i 's@-fconserve-stack@@g' compile_commands.json
sed -i 's@-mabi=lp64@@g' compile_commands.json

# 检查数据正确性 - 4K为单位写数据，但是有的不会被写/换出到硬盘，必须超过物理内存，第一块才会被写到硬盘
# 一个新的页写时，换出时不会被读，只写回之前的页
./a.out 9
sync; echo 3 > /proc/sys/vm/drop_caches
hexdump -n 512 /dev/vdb
hexdump -n 512 -s 4096 /dev/vdb