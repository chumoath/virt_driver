#include "pcie.h"

static struct pcie_vmem_desc pcie_vmem_descs[VIRTUAL_SIZE/PAGE_SIZE];
static struct pcie_phymem_desc pcie_phymem_descs[PHY_SIZE/PAGE_SIZE];

int pcie_init_vmem(void)
{
    int i;
    for (i = 0; i < VIRTUAL_SIZE / PAGE_SIZE; i++) {
        pcie_vmem_descs[i].status = VPAGE_NO_MAP;
        pcie_vmem_descs[i].swp = &swap_entries[i];
        pcie_vmem_descs[i].id = i;
        pcie_vmem_descs[i].phy_desc = NULL;
    }
    return 0;
}

int pcie_init_phymem(void)
{
    int i;
    unsigned long pfn = RESERVED_PHYS_ADDR >> PAGE_SHIFT;
    for (i = 0; i < PHY_SIZE / PAGE_SIZE; i++) {
        struct page *page = pfn_to_page(pfn);
        pcie_phymem_descs[i].pfn = pfn++;
        pcie_phymem_descs[i].status = PPAGE_FREE;
        pcie_phymem_descs[i].dirty = false;
        init_page_count(page);
        page_mapcount_reset(page);
    }
    return 0;
}

int get_vaddr_pte(struct vm_area_struct *vma, unsigned long vaddr)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;
    pte_t pte;
    spinlock_t *ptl;

    pgd = pgd_offset(vma->vm_mm, vaddr);
    p4d = p4d_offset(pgd, vaddr);
    pud = pud_offset(p4d, vaddr);
    pmd = pmd_offset(pud, vaddr);
    ptep = pte_offset_map_lock(vma->vm_mm, pmd, vaddr, &ptl);
    if (!ptep) return -EINVAL;
    pte = ptep_get(ptep);
    pte_unmap_unlock(ptep, ptl);
    pr_info("get vaddr 0x%lx pte is 0x%llx\n", vaddr, pte.pte);
    
    return 0;
}

static int fill_vpage(struct vm_area_struct *vma, struct pcie_vmem_desc *cur_desc)
{
    int i;
    struct pcie_vmem_desc *vmem_desc = NULL;
    struct pcie_phymem_desc *phymem_desc = NULL;

    for (i = 0; i < PHY_SIZE/PAGE_SIZE; i++) {
        if (pcie_phymem_descs[i].status == PPAGE_FREE) {
            phymem_desc = &pcie_phymem_descs[i];
            break;
        }
    }

    if (phymem_desc) {
        cur_desc->phy_desc = phymem_desc;
        if (cur_desc->status == VPAGE_IN_SWAP) {
            my_swap_pagein(vma, cur_desc);
        }
        phymem_desc->status = PPAGE_IN_USE;
        cur_desc->status = VPAGE_IN_MEM;
        return 0;
    }

    for (i = 0; i < VIRTUAL_SIZE/PAGE_SIZE; i++) {
        if (pcie_vmem_descs[i].status == VPAGE_IN_MEM) {
            if (!pcie_vmem_descs[i].phy_desc) {
                BUG();
                return VM_FAULT_OOM;
            }
            vmem_desc = &pcie_vmem_descs[i];
            break;
        }
    }

    if (vmem_desc == NULL) {
        BUG();
        return VM_FAULT_OOM;
    }

    evict_page(vma, vmem_desc, cur_desc);
    return 0;
}

vm_fault_t pcieBase_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    unsigned long offset = vmf->address - vma->vm_start;
    unsigned long vpage_idx = offset / PAGE_SIZE;
    struct pcie_vmem_desc *vdesc = &pcie_vmem_descs[vpage_idx];
    int ret = 0;
    struct page *page;

    if (vdesc->status == VPAGE_IN_SWAP || vdesc->status == VPAGE_NO_MAP) {
        if (vdesc->phy_desc) {
            BUG();
            return VM_FAULT_SIGBUS;
        }
        ret = fill_vpage(vma, vdesc);
    }

    if (vdesc->status == VPAGE_IN_MEM) {
        if (!vdesc->phy_desc) {
            BUG();
            return VM_FAULT_SIGBUS;
        } else {
            page = pfn_to_page(vdesc->phy_desc->pfn);
            get_page(page);
            vmf->page = page;
        }
    }

    return ret;
}