#include "pcie.h"

static int pciebase_init(void)
{
    setup_swap_mapping();
    pcie_init_phymem();
    pcie_init_vmem();
    return pciebase_cdev_init();
}

static void pciebase_exit(void)
{
    pciebase_cdev_clean();
    cleanup_swap_mapping();
}

// late_initcall(pciebase_init)
module_init(pciebase_init)
module_exit(pciebase_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chumoath");