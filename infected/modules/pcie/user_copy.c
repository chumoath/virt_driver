#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#define PCIE_CMD_INIT     _IO('P', 1)
#define PCIE_CMD_RESET    _IO('P', 2)
#define PCIE_CMD_GET_PTE  _IO('P', 3)

long strToLong(const char *str)
{
    char *endptr;
    long int num;
    num = strtol(str, &endptr, 10);

    if (errno == ERANGE) {
        printf ("range error\n");
        exit(-1);
    } else if (str == endptr) {
        printf ("no convert\n");
        exit(-2);
    } else if (*endptr != '\0') {
        printf ("invalid str: %s\n", endptr);
        exit(-3);
    }

    return num;
}

int main(int argc, char **argv)
{
    int fd;
    void *addr;
    char buf[4096];
    size_t mmap_size = (32UL << 30);
    long data_size_GB;
    size_t data_size;
    size_t total_size;
    char *tmp;
    struct timeval time;
    long before_w, after_w, before_r, after_r;

    if (argc != 2) {
        printf ("usage: %s <data size(GB)>\n", argv[0]);
        exit(1);
    }

    data_size_GB = strToLong(argv[1]);
    if (data_size_GB <= 0) {
        printf ("usage: %s <data size(GB)>\n", argv[0]);
        exit(2);
    }

    data_size = (data_size_GB << 30);
    printf ("mmap_size = %ldGB\n", mmap_size >> 30);
    printf ("data_size = %ldGB\n", data_size >> 30);

    fd = open("/dev/pcieBase", O_RDWR | O_SYNC);
    addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf ("addr = %p\n", addr);

    // 1.reset pcie mm
    ioctl(fd, PCIE_CMD_RESET, NULL);

    gettimeofday(&time, NULL);
    before_w = time.tv_sec * 1000000 + time.tv_usec;
    printf ("before_w: %ld\n", before_w);

    // 2. copy mem to vmem
    tmp = addr;
    total_size = 0;
    while (total_size != data_size) {
        memcpy(tmp, buf, sizeof(buf));
        tmp += sizeof(buf);
        total_size += sizeof(buf);
    }

    gettimeofday(&time, NULL);
    after_w = time.tv_sec * 1000000 + time.tv_usec;
    printf ("after_w : %ld\n", after_w);


    gettimeofday(&time, NULL);
    before_r = time.tv_sec * 1000000 + time.tv_usec;
    printf ("before_r: %ld\n", before_r);

    // 3. copy vmem to mem
    tmp = addr;
    total_size = 0;
    while (total_size != data_size) {
        memcpy(buf, tmp, sizeof(buf));
        tmp += sizeof(buf);
        total_size += sizeof(buf);
    }

    gettimeofday(&time, NULL);
    after_r = time.tv_sec * 1000000 + time.tv_usec;
    printf ("after_r : %ld\n", after_r);

    printf ("write_%ldG: %.3fs\n", data_size >> 30, (after_w - before_w)/1000000.);
    printf ("read_%ldG : %.3fs\n", data_size >> 30, (after_r - before_r)/1000000.);

    munmap(addr, mmap_size);
    close(fd);
    return 0;
}