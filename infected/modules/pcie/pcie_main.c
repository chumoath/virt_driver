#include "pcie.h"

static int pciebase_init(void)
{
    pcie_init_phymem();
    pcie_init_vmem();
    init_swap_device();
    return pciebase_cdev_init();
}

static void pciebase_exit(void)
{
    cleanup_swap_device();
    pciebase_cdev_clean();
}

module_init(pciebase_init)
module_exit(pciebase_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chumoath");