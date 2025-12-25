#pragma once

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pgtable.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/swapops.h>
#include <linux/swapfile.h>
#include <linux/mm_types.h>
#include <asm/pgalloc.h>
#include <linux/suspend.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define PCIE_CMD_INIT     _IO('P', 1)
#define PCIE_CMD_RESET    _IO('P', 2)
#define PCIE_CMD_GET_PTE  _IO('P', 3)

#define PCIE_BASE_NAME    "pcieBase"

#define VIRTUAL_SIZE_GB  2UL
#define VIRTUAL_SIZE     (VIRTUAL_SIZE_GB << 30)

#define SWAP_SIZE_GB     2UL
#define SWAP_SIZE        (SWAP_SIZE_GB << 30)

#define PHY_SIZE_GB      1UL
#define PHY_SIZE         (PHY_SIZE_GB << 30)

#define MY_PAGE_SIZE     (4UL << 20)

#define RESERVED_PHYS_ADDR  0x50000000

struct PCIeAdapter {
    struct cdev cdev;
    struct class *class;
    dev_t major;
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
};

struct pcie_vmem_desc {
    struct pcie_phymem_desc *phy_desc;
    int status;
    swp_entry_t *swp;
    unsigned long id;
};

int init_swap_device(void);
void cleanup_swap_device(void);

int pcie_init_vmem(void);
int pcie_init_phymem(void);

int get_vaddr_pte(struct vm_area_struct *vma, unsigned long vaddr);
vm_fault_t pcieBase_fault(struct vm_fault *vmf);
int pciebase_cdev_init(void);
void pciebase_cdev_clean(void);

int evict_page(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc, struct pcie_vmem_desc *swap_desc);
int my_swap_pagein(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc);