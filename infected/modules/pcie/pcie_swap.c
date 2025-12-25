#include "pcie.h"

int pciebase_swapdev_init(struct PCIeAdapter *pcie_adap)
{
    loff_t total_bytes;
    pcie_adap->swap_device_path = "/dev/vdb";
	pcie_adap->swap_bdev = blkdev_get_by_path(pcie_adap->swap_device_path, FMODE_READ | FMODE_WRITE, NULL);
	if (IS_ERR(pcie_adap->swap_bdev)) {
		pr_warn("pNFS: failed to open device %s (%ld)\n",
			pcie_adap->swap_device_path, PTR_ERR(pcie_adap->swap_bdev));
        return -ENODEV;
	}
    
    total_bytes = i_size_read(pcie_adap->swap_bdev->bd_inode);
    pr_info("Swap device: %s, sector size: %u, capacity: %lluM\n", pcie_adap->swap_device_path,
                    bdev_logical_block_size(pcie_adap->swap_bdev), total_bytes >> 20);    
    return 0;
}

void pciebase_swapdev_clean(struct PCIeAdapter *pcie_adap)
{
    if (pcie_adap->swap_bdev) {
        blkdev_put(pcie_adap->swap_bdev, FMODE_READ | FMODE_WRITE);
    }
}

static int my_swap_writepages(struct pcie_vmem_desc *desc)
{
	struct bio *bio;
	int ret = 0;
    struct block_device *bdev = g_pcie_adap.swap_bdev;
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
    struct block_device *bdev = g_pcie_adap.swap_bdev;
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