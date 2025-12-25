#include "pcie.h"

static int pcieBase_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int pcieBase_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static long pcieBase_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
    struct vm_area_struct *vma;
    unsigned long vaddr;
    switch (cmd) {
        case PCIE_CMD_RESET:
            break;
        case PCIE_CMD_GET_PTE:
            vaddr = arg;
            vma = find_vma(current->mm, vaddr);
            get_vaddr_pte(vma, vaddr);
            break;
        default:
        ;
    }

    return 0;
}

static const struct vm_operations_struct pcieBase_vm_ops = {
    .fault = pcieBase_fault,
};

static int pcieBase_mmap(struct file *file, struct vm_area_struct *vma)
{
    printk ("vma_start = 0x%lx, vma_end = 0x%lx, vm_pgoff=0x%lx\n", vma->vm_start, vma->vm_end, vma->vm_pgoff);
    if (vma->vm_end - vma->vm_start != VIRTUAL_SIZE) {
        pr_err("Invalid mmap size\n");
        return -EINVAL;
    }
    vma->vm_flags |= VM_PFNMAP;
    vma->vm_ops = &pcieBase_vm_ops;
    return 0;
}

static struct file_operations pcieBase_fops = {
    .owner = THIS_MODULE,
    .open = pcieBase_open,
    .release = pcieBase_close,
    .unlocked_ioctl = pcieBase_ioctl,
    .mmap = pcieBase_mmap,
};

int pciebase_cdev_init(struct PCIeAdapter *pcie_adap)
{
    int ret;
    dev_t dev_id = 0, tmp;
    struct device *dev = NULL;

    ret = alloc_chrdev_region(&dev_id, 0, 1, "pciebase");
    if (ret < 0) {
        return -EFAULT;
    }
    pcie_adap->major = MAJOR(dev_id);
    cdev_init(&pcie_adap->cdev, &pcieBase_fops);
    pcie_adap->cdev.owner = THIS_MODULE;

    ret = cdev_add(&pcie_adap->cdev, dev_id, 1);
    if (ret < 0) {
        goto _cdev_add_err;
    }

    pcie_adap->class = class_create(THIS_MODULE, PCIE_BASE_NAME);
    if (IS_ERR(pcie_adap->class)) {
        ret = -ENXIO;
        goto _class_create_err;
    }

    tmp = MKDEV(pcie_adap->major, 0);
    dev = device_create(pcie_adap->class, NULL, tmp, NULL, PCIE_BASE_NAME);
    if (IS_ERR(dev)) {
        ret = -EIO;
        goto _device_create_err;
    }
    return 0;

_device_create_err:
    class_destroy(pcie_adap->class);
_class_create_err:
    cdev_del(&pcie_adap->cdev);
_cdev_add_err:
    unregister_chrdev_region(dev_id, 1);

    return ret;
}

void pciebase_cdev_clean(struct PCIeAdapter *pcie_adap)
{
    device_destroy(pcie_adap->class, MKDEV(pcie_adap->major, 0));
    class_destroy(pcie_adap->class);
    cdev_del(&pcie_adap->cdev);
    unregister_chrdev_region(MKDEV(pcie_adap->major, 0), 1);
}