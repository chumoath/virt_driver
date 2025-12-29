#include "pcie.h"

static int proc_pcie_show(struct seq_file *m, void *v)
{
    seq_printf(m, "total_read: %ld\n", atomic_long_read(&g_pcie_adap.total_read));
    seq_printf(m, "total_write: %ld\n", atomic_long_read(&g_pcie_adap.total_write));
    return 0;
}

int proc_pcie_init(struct PCIeAdapter *pcie_adap)
{
    pcie_adap->base_dir = proc_mkdir("pcie_mm", NULL);
    proc_create_single("total_swap", 0, pcie_adap->base_dir, proc_pcie_show);
    return 0;
}

int proc_pcie_clean(struct PCIeAdapter *pcie_adap)
{
    int ret = 0;
    ret = remove_proc_subtree("pcie_mm", NULL);
    return ret;
}