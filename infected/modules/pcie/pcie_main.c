#include "pcie.h"

struct PCIeAdapter g_pcie_adap;

static int pciebase_init(void)
{
    pcie_mm_reset();
    pciebase_swapdev_init(&g_pcie_adap);
    proc_pcie_init(&g_pcie_adap);
    return pciebase_cdev_init(&g_pcie_adap);
}

static void pciebase_exit(void)
{
    proc_pcie_clean(&g_pcie_adap);
    pciebase_cdev_clean(&g_pcie_adap);
    pciebase_swapdev_clean(&g_pcie_adap);
}

module_init(pciebase_init)
module_exit(pciebase_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chumoath");