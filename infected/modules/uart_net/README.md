
# guest
```shell
ping 192.168.3.1 -c 1

/busybox telnetd
/busybox httpd

# vm1
/busybox httpd -h /

# vm2
wget 192.168.3.1/home/root/config_vm1.sh

# telnet/ssh问题: 传输太慢导致软件卡死.

netstat -s

ip link set dev uartnet0 down
```

# qemu
```shell
apt install -y ninja-build
apt install -y pkg-config
apt install -y libglib2.0-dev
apt install -y libpixman-1-dev
mkdir build && cd build
../configure --target-list=aarch64-softmmu
ninja

# dts
qemu-system-aarch64 -M virt,gic-version=3,dumpdtb=arm64.dtb -nographic -m 1024M -cpu cortex-a72 -smp 2 -kernel arch/arm64/boot/Image -drive format=raw,file=/root/core-image-full-cmdline-qemuarm64.rootfs.ext4 -append "console=ttyAMA0 root=/dev/vda rw nokaslr" -net nic,netdev=tap0,model=virtio -netdev tap,id=tap0,ifname=tap0,script=no,downscript=no
dtc -I dtb -O dts -o arm64.dts arm64.dtb
dtc -I dts -O dtb -o arm64.dtb arm64.dts


# socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

```shell
# 两个pl011根据在设备树中的顺序，为ttyAMA0/1
vm1
-serial unix:/tmp/vm-socket,server,nowait

vm2
-serial unix:/tmp/vm-socket

diff --color -r qemu-6.2.0 qemu-6.2.0.new


ssh root@192.168.0.2
```

# linux-5.10.240

```shell
apt install -y gcc-aarch64-linux-gnu
apt install -y qemu-system-arm
apt install -y bison
apt install -y libssl-dev
apt install -y device-tree-compiler
apt install -y gdb-multiarch

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make distclean
make defconfig
make menuconfig
make Image -j$(nproc)
# 否则编译模块会报错： scripts/module.lds
make modules -j$(nproc)
# 去掉 -mabi=lp64，查看clangd日志看错误，修复即可
./scripts/clang-tools/gen_compile_commands.py
make scripts_gdb

make help

cp -fa vmlinux.symvers Module.symvers


gdb-multiarch vmlinux
target remote :1234
lx-symbols
```

# rootfs
https://downloads.yoctoproject.org/releases/yocto/yocto-5.2.2/machines/qemu/qemuarm64/core-image-full-cmdline-qemuarm64.rootfs.ext4


# busybox
```shell
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make defconfig
#static   no shared libs
make menuconfig
make -j$(nproc)
```

# ip
- vm1 192.168.33.0/24
- vm2 192.168.0.0/24
- uart 192.168.3.0/24
