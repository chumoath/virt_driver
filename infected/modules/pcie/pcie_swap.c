#include "pcie.h"

swp_entry_t *swap_entries;

int setup_swap_mapping(void)
{
    int i;
    int ret;

    swap_entries = kmalloc_array(SWAP_PAGE_COUNT, sizeof(swp_entry_t), GFP_KERNEL);
    if (!swap_entries) return -ENOMEM;

    for (i = 0; i < SWAP_PAGE_COUNT; i++) {
        ret = get_swap_pages(1, &swap_entries[i], 1);
        if (ret != 1) {
            pr_err("Failed to get swap page at index %d\n", i);
            while (--i >= 0) {
                swap_free(swap_entries[i]);
            }
            kfree(swap_entries);
            return -ENOSPC;
        }
    }
    printk("Allocated %ld swp_entry_t ok\n", SWAP_PAGE_COUNT);

    return 0;
}

int cleanup_swap_mapping(void)
{
    int i = SWAP_PAGE_COUNT;
    if (!swap_entries) return 0;

    while (--i >= 0) {
        swap_free(swap_entries[i]);
    }
    kfree(swap_entries);
    printk ("freed %ld swp_entry_t ok\n", SWAP_PAGE_COUNT);

    return 0;
}

static void swap_io_end_io(struct bio *bio)
{
    struct task_struct *waiter = bio->bi_private;

    WRITE_ONCE(bio->bi_private, NULL);
    bio_put(bio);
    if (waiter) {
        blk_wake_io_task(waiter);
        put_task_struct(waiter);
    }
}

static int my_swap_writepage(struct page *page, swp_entry_t entry)
{
	struct bio *bio;
	int ret = 0;
	struct swap_info_struct *sis;
    struct block_device *bdev;
	struct gendisk *disk;
    blk_qc_t qc;
    sector_t sector;

    sis = get_swap_device(entry);
    if (!sis || !sis->bdev) {
        pr_err("No swap device found\n");
        return -ENODEV;
    }

    bdev = sis->bdev;
    sector = swp_offset(entry) * (PAGE_SIZE >> SECTOR_SHIFT);
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) return -ENOMEM;

    bio_set_dev(bio, bdev);
    bio->bi_iter.bi_sector = sector;
    bio->bi_opf = GFP_NOIO | REQ_OP_WRITE | REQ_SWAP | REQ_SYNC;
    bio->bi_end_io = swap_io_end_io;

    bio_add_page(bio, page, PAGE_SIZE, 0);
    disk = bio->bi_disk;
    bio->bi_opf |= REQ_HIPRI;
    get_task_struct(current);
    bio->bi_private = current;
	bio_get(bio);
	qc = submit_bio(bio);
	while (true) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (!READ_ONCE(bio->bi_private))
			break;

		if (!blk_poll(disk->queue, qc, true))
			blk_io_schedule();
	}
	__set_current_state(TASK_RUNNING);
	bio_put(bio);

	return ret;
}

static int my_swap_readpage(struct page *page, swp_entry_t entry)
{
	struct bio *bio;
	int ret = 0;
	struct swap_info_struct *sis;
    struct block_device *bdev;
	struct gendisk *disk;
    blk_qc_t qc;
    sector_t sector;

    sis = get_swap_device(entry);
    if (!sis || !sis->bdev) {
        pr_err("No swap device found\n");
        return -ENODEV;
    }

    bdev = sis->bdev;
    sector = swp_offset(entry) * (PAGE_SIZE >> SECTOR_SHIFT);
    bio = bio_alloc(GFP_KERNEL, 1);
    if (!bio) return -ENOMEM;

    bio_set_dev(bio, bdev);
    bio->bi_iter.bi_sector = sector;
    bio->bi_end_io = swap_io_end_io;

    bio_add_page(bio, page, PAGE_SIZE, 0);
    disk = bio->bi_disk;

    bio_set_op_attrs(bio, REQ_OP_READ, 0);
    bio->bi_opf |= REQ_HIPRI;
    get_task_struct(current);
    bio->bi_private = current;
	bio_get(bio);
	qc = submit_bio(bio);
	while (true) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (!READ_ONCE(bio->bi_private))
			break;

		if (!blk_poll(disk->queue, qc, true))
			blk_io_schedule();
	}
	__set_current_state(TASK_RUNNING);
	bio_put(bio);

	return ret;
}

static unsigned long read_page(struct pcie_vmem_desc *swap_desc)
{
    struct page *page = pfn_to_page(swap_desc->phy_desc->pfn);
    my_swap_readpage(page, *swap_desc->swp);
    swap_desc->status = VPAGE_IN_MEM;

    return 0;
}

static unsigned long write_page(struct pcie_vmem_desc *swap_desc)
{
    struct page *page = pfn_to_page(swap_desc->phy_desc->pfn);
    my_swap_writepage(page, *swap_desc->swp);
    swap_desc->status = VPAGE_IN_SWAP;

    return 0;
}

int evict_page(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc, struct pcie_vmem_desc *swap_desc)
{
    struct page *page = pfn_to_page(vmem_desc->phy_desc->pfn);

    lock_page(page);

    write_page(vmem_desc);
    zap_vma_ptes(vma, vma->vm_start + vmem_desc->id * PAGE_SIZE, PAGE_SIZE);
    swap_desc->phy_desc = vmem_desc->phy_desc;
    vmem_desc->phy_desc = NULL;

    read_page(swap_desc);
    unlock_page(page);

    // put_page(page);
    return 0;
}

int my_swap_pagein(struct vm_area_struct *vma, struct pcie_vmem_desc *vmem_desc)
{
    struct page *page = pfn_to_page(vmem_desc->phy_desc->pfn);
    lock_page(page);
    read_page(vmem_desc);
    unlock_page(page);

    return 0;
}