#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>

#define PCIE_CMD_GET_PTE  _IO('P', 3)

int main(int argc, char **argv)
{
    int fd, src_fd, dst_fd;
    void *addr;
    char buf[4096];
    size_t size = 0x80000000;
    size_t read_n, write_n;
    size_t total_size;
    char *tmp;

    fd = open("/dev/pcieBase", O_RDWR | O_SYNC);
    src_fd = open("/root/test.img", O_RDWR);
    dst_fd = open("/root/test_dst.img", O_RDWR | O_CREAT);
    addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf ("addr = %p\n", addr);

    tmp = addr;
    total_size = 0;

    while (read_n = read(src_fd, buf, 4096)) {
        memcpy(tmp, buf, read_n);
        tmp += read_n;
        total_size += read_n;
    }

    int cmd = PCIE_CMD_GET_PTE;
    ioctl(fd, cmd, addr);
    ioctl(fd, cmd, addr+4096);
    printf ("total_size = 0x%lx\n", total_size);
    printf ("copy src to mem ok\n");

    system("echo 3 > /proc/sys/vm/drop_caches");

    write_n = write(dst_fd, addr, total_size);
    printf ("write_n = 0x%lx\n", write_n);

    printf ("copy mem to dst ok\n");
    munmap(addr, size);
    close(fd);
    close(src_fd);
    close(dst_fd);
    return 0;
}