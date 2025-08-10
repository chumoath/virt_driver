ip tuntap add tap1 mode tap group 0
ip link set dev tap1 up
ip addr add 192.168.0.1/24 dev tap1
iptables -A POSTROUTING -t nat -j MASQUERADE -s 192.168.0.0/24
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
    -drive format=raw,file=$bin_path/vm2.ext4 \
    -append "tty=ttyAMA0 root=/dev/vda rw nokaslr" \
    -net nic,netdev=tap1,model=virtio -netdev tap,id=tap1,ifname=tap1,script=no,downscript=no \
    -serial telnet::55556,server,nowait,nodelay \
    -serial unix:/tmp/vm-socket \
    -monitor none &
cd -

sleep 3
telnet localhost 55556
