#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("pci driver for Qemu created device ldd");
MODULE_LICENSE("GPL");

#define DRIVER_NAME             "pci_drv_qemu"
#define EVENT_IRQ_VECTOR        (0)
#define PCI_DEVICE_MEMIO_BAR    (0)
#define PCI_DEVICE_PRTIO_BAR    (1)
#define DRV_MSI                 "pci Qemu MSI"

struct pci_prv_dev {
    u8 __iomem *hwmem;
    u8 __iomem *hwprtmem;
};

static struct pci_device_id pci_dev_table[] = {
    {PCI_DEVICE(0x1234, 0xAABB)},
    {0,}
};
MODULE_DEVICE_TABLE(pci, pci_dev_table);

static ssize_t mmioval_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
    long val;
    int ret;
    struct pci_prv_dev *prv_dev =
        (struct pci_prv_dev *) dev_get_drvdata(dev);

    ret = kstrtol(buf, 10, &val);
	if(ret)
		return ret;
    iowrite32(val, prv_dev->hwmem);

    return count;
}

static ssize_t prtioval_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
    long val;
    int ret;
    struct pci_prv_dev *prv_dev =
        (struct pci_prv_dev *) dev_get_drvdata(dev);

    ret = kstrtol(buf, 10, &val);
	if(ret)
		return ret;
    iowrite32(val, prv_dev->hwprtmem);

    return count;
}

static ssize_t mmioval_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
    unsigned int val;
    struct pci_prv_dev *prv_dev =
        (struct pci_prv_dev *) dev_get_drvdata(dev);
    
    val = ioread32(prv_dev->hwmem);
    return sprintf(buf, "mmioval show: %u\n", val);
}

static ssize_t prtioval_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
    unsigned int val;
    struct pci_prv_dev *prv_dev =
        (struct pci_prv_dev *) dev_get_drvdata(dev);
    
    val = ioread32(prv_dev->hwprtmem);
    return sprintf(buf, "prtioval show : %u\n", val);
}

static DEVICE_ATTR_RW(mmioval);
static DEVICE_ATTR_RW(prtioval);

static struct attribute * pcidrv_attrs[] = {
	&dev_attr_mmioval.attr,
	&dev_attr_prtioval.attr,
	NULL
};

static struct attribute_group pcidrv_attr_grp = {
	.attrs = pcidrv_attrs
};

static irqreturn_t irq_drv_hndl(int irq, void *prv)
{
   (void) prv;
   pr_info("Handle IRQ #%d: Write is done\n", irq);
   return IRQ_HANDLED;
}

static int pci_drv_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int ret;
    u16 vid, pid;
    struct pci_prv_dev *prv_dev;

    pci_read_config_word(pdev, PCI_VENDOR_ID, &vid);
    pci_read_config_word(pdev, PCI_DEVICE_ID, &pid);
    dev_info(&pdev->dev, "New pci device vid: 0x%X pid: 0x%X\n", vid, pid);

    ret = pci_enable_device_mem(pdev);
    
    if(ret)
        return ret;

    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(ret < 0)
        goto err_dma_set;
    pci_set_master(pdev);

    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret)
        goto err_req_rgn;

    prv_dev = devm_kzalloc(&pdev->dev,
        sizeof(struct pci_prv_dev), GFP_KERNEL);
    if (!prv_dev) {
        ret = -ENOMEM;
        goto err_alloc;
    }

    prv_dev->hwmem = pci_iomap(pdev, PCI_DEVICE_MEMIO_BAR,
        pci_resource_len(pdev, PCI_DEVICE_MEMIO_BAR));
    prv_dev->hwprtmem = pci_iomap(pdev, PCI_DEVICE_PRTIO_BAR,
        pci_resource_len(pdev, PCI_DEVICE_PRTIO_BAR));
    if (!prv_dev->hwmem || !prv_dev->hwprtmem) {
       ret = -EIO;  
       goto err_iomap;
    }

    /* 1 msi line */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0)
        goto err_irq_vec;

    ret = request_irq(pci_irq_vector(pdev, EVENT_IRQ_VECTOR),
        irq_drv_hndl, 0, DRV_MSI, pdev);
    if(ret) {
        goto err_req_irq;
    }

	ret = sysfs_create_group(&pdev->dev.kobj, &pcidrv_attr_grp);
	if (ret)
        goto err_sysfs_grp;

    pci_set_drvdata(pdev, prv_dev);
    dev_set_drvdata(&pdev->dev, prv_dev);

    return 0;

err_sysfs_grp:
    free_irq(pci_irq_vector(pdev, EVENT_IRQ_VECTOR), pdev);
err_req_irq:
    pci_free_irq_vectors(pdev);
err_irq_vec:
    pci_iounmap(pdev, prv_dev->hwmem);
    pci_iounmap(pdev, prv_dev->hwprtmem);
err_iomap:
err_alloc:
    pci_release_regions(pdev);
err_dma_set:
err_req_rgn:
    pci_disable_device(pdev);

    return ret;
}

static void pci_drv_remove(struct pci_dev *pdev)
{
    struct pci_prv_dev *prv_dev =
        (struct pci_prv_dev *)pci_get_drvdata(pdev);

    sysfs_remove_group(&pdev->dev.kobj, &pcidrv_attr_grp);
    free_irq(pci_irq_vector(pdev, EVENT_IRQ_VECTOR), pdev);
    pci_free_irq_vectors(pdev);
    pci_iounmap(pdev, prv_dev->hwmem);
    pci_iounmap(pdev, prv_dev->hwprtmem);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    dev_info(&pdev->dev, "pci device removed");
}

static struct pci_driver pci_driver = {
    .name = DRIVER_NAME,
    .id_table = pci_dev_table,
    .probe = pci_drv_probe,
    .remove = pci_drv_remove
};

module_pci_driver(pci_driver);

