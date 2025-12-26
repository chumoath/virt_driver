qemu-img create -f qcow2 swap.qcow2 4G
qemu-img convert -f raw -O qcow2 swap.img swap.qcow2
qemu-img convert -f raw -O qcow2 ubuntu22_arm64.img ubuntu22_arm64.qcow2
telnet localhost 55555

dmesg -n 7
mkswap /dev/vdb
swapon /dev/vdb

savevm swap
-loadvm swap

dd if=/dev/urandom of=test.img bs=1G count=2
scp ./pcie_drv.ko 192.168.33.2:/root/

qemu-system-aarch64 -M virt,gic-version=3 -m 16G -cpu cortex-a72 -smp 4 -kernel Image -dtb arm64.dtb  \
                    -append "console=ttyAMA0 nokaslr root=/dev/vda rw" \
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

addr2line -e  build.arm64/vmlinux FFFF8000104506F0

# 2G - 4K
dd if=test.img of=xx bs=4K count=524287

sed -i 's@-fno-allow-store-data-races@@g' compile_commands.json
sed -i 's@-femit-struct-debug-baseonly@@g' compile_commands.json
sed -i 's@-fconserve-stack@@g' compile_commands.json
sed -i 's@-mabi=lp64@@g' compile_commands.json