#include "pcie.h"

static struct block_device *swap_bdev = NULL;
static char *swap_device_path = "/dev/vdb";

int init_swap_device(void)
{
    swap_bdev = lookup_bdev(swap_device_path);
    if (!swap_bdev) {
        pr_err("Cannot find block device %s\n", swap_device_path);
        return -ENODEV;
    }
    
    pr_info("Swap device: %s\n", swap_device_path);
    return 0;
}

void cleanup_swap_device(void)
{
    if (swap_bdev) {
        bdput(swap_bdev);
        swap_bdev = NULL;
    }
}

static int my_swap_writepages(struct pcie_vmem_desc *desc)
{
	struct bio *bio;
	int ret = 0;
    struct block_device *bdev = swap_bdev;
    sector_t sector;
    int i;
    unsigned long pfn = desc->phy_desc->pfn;

    sector = (desc->id * MY_PAGE_SIZE) >> SECTOR_SHIFT;

    bio = bio_kmalloc(GFP_NOIO, MY_PAGE_SIZE/PAGE_SIZE);
	if (!bio)
		return -ENOMEM;

	bio_set_dev(bio, bdev);
	bio->bi_opf = REQ_OP_WRITE | REQ_SWAP | REQ_SYNC | REQ_HIPRI;
	bio->bi_iter.bi_sector = sector;

	for (i = 0; i < MY_PAGE_SIZE/PAGE_SIZE; ++i) {
        struct page *page = pfn_to_page(pfn);
        ++pfn;

		if (!bio_add_page(bio, page, PAGE_SIZE, 0)) {
			return -EIO;
		}
	}

	ret = submit_bio_wait(bio);
	bio_put(bio);

	return ret;
}

static int my_swap_readpages(struct pcie_vmem_desc *desc)
{
	struct bio *bio;
	int ret = 0;
    struct block_device *bdev = swap_bdev;
    sector_t sector;
    int i;
    unsigned long pfn = desc->phy_desc->pfn;

    sector = (desc->id * MY_PAGE_SIZE) >> SECTOR_SHIFT;

    bio = bio_kmalloc(GFP_NOIO, MY_PAGE_SIZE/PAGE_SIZE);
	if (!bio)
		return -ENOMEM;

	bio_set_dev(bio, bdev);
	bio->bi_opf = REQ_OP_READ | REQ_SYNC | REQ_HIPRI;
	bio->bi_iter.bi_sector = sector;

	for (i = 0; i < MY_PAGE_SIZE/PAGE_SIZE; ++i) {
        struct page *page = pfn_to_page(pfn);
        ++pfn;

		if (!bio_add_page(bio, page, PAGE_SIZE, 0)) {
            bio_put(bio);
			return -EIO;
		}
	}

	ret = submit_bio_wait(bio);
	bio_put(bio);

	return ret;
}

static unsigned long read_pages(struct pcie_vmem_desc *swap_desc)
{
    my_swap_readpages(swap_desc);
    swap_desc->status = VPAGE_IN_MEM;

    return 0;
}

static unsigned long write_pages(struct pcie_vmem_desc *swap_desc)
{
    my_swap_writepages(swap_desc);
    swap_desc->status = VPAGE_IN_SWAP;

    return 0;
}

int evict_page(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc, struct pcie_vmem_desc *swap_desc)
{
    struct page *page = pfn_to_page(vmem_desc->phy_desc->pfn);

    lock_page(page);

    write_pages(vmem_desc);
    zap_vma_ptes(vma, vma->vm_start + vmem_desc->id * MY_PAGE_SIZE, MY_PAGE_SIZE);
    swap_desc->phy_desc = vmem_desc->phy_desc;
    vmem_desc->phy_desc = NULL;

    read_pages(swap_desc);
    unlock_page(page);

    // put_page(page);
    return 0;
}

int my_swap_pagein(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc)
{
    struct page *page = pfn_to_page(vmem_desc->phy_desc->pfn);
    lock_page(page);
    read_pages(vmem_desc);
    unlock_page(page);

    return 0;
}