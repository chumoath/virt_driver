#pragma once

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pgtable.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <asm/pgalloc.h>
#include <linux/suspend.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/version.h>
#include <linux/proc_fs.h>

#define PCIE_CMD_INIT     _IO('P', 1)
#define PCIE_CMD_RESET    _IO('P', 2)
#define PCIE_CMD_GET_PTE  _IO('P', 3)

#define PCIE_BASE_NAME    "pcieBase"

#define VIRTUAL_SIZE_GB  32UL
#define VIRTUAL_SIZE     (VIRTUAL_SIZE_GB << 30)

#define SWAP_SIZE_GB     32UL
#define SWAP_SIZE        (SWAP_SIZE_GB << 30)

#define PHY_SIZE_GB      8UL
#define PHY_SIZE         (PHY_SIZE_GB << 30)

#define MY_PAGE_SIZE     (4UL << 20)

#define RESERVED_PHYS_ADDR  0x400000000

struct PCIeAdapter {
    struct cdev cdev;
    struct class *class;
    dev_t major;
    struct block_device *swap_bdev;
    char *swap_device_path;
    atomic_long_t total_write;
    atomic_long_t total_read;
    struct proc_dir_entry *base_dir;
};

enum {
    VPAGE_IN_MEM,
    VPAGE_IN_SWAP,
    VPAGE_NO_MAP,
};

enum {
    PPAGE_FREE,
    PPAGE_IN_USE,
    PPAGE_SWAPCACHE,
};

struct pcie_phymem_desc {
    int status;
    unsigned long pfn;
    bool dirty;
    atomic_long_t write_cnt;
    atomic_long_t read_cnt;
};

struct pcie_vmem_desc {
    struct pcie_phymem_desc *phy_desc;
    int status;
    swp_entry_t *swp;
    unsigned long id;
};

extern struct PCIeAdapter g_pcie_adap;

int pciebase_swapdev_init(struct PCIeAdapter *pcie_adap);
void pciebase_swapdev_clean(struct PCIeAdapter *pcie_adap);

int pcie_mm_reset(void);

int get_vaddr_pte(struct vm_area_struct *vma, unsigned long vaddr);
vm_fault_t pcieBase_fault(struct vm_fault *vmf);

int pciebase_cdev_init(struct PCIeAdapter *pcie_adap);
void pciebase_cdev_clean(struct PCIeAdapter *pcie_adap);

int evict_page(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc, struct pcie_vmem_desc *swap_desc);
int my_swap_pagein(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc);

int proc_pcie_init(struct PCIeAdapter *pcie_adap);
int proc_pcie_clean(struct PCIeAdapter *pcie_adap);