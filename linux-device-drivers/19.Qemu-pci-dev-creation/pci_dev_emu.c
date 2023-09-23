#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "qemu/units.h"
#include "hw/irq.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pci_host.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/qemu-file-types.h"
#include "migration/vmstate.h"
#include "net/net.h"
#include "sysemu/numa.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "hw/loader.h"
#include "qemu/error-report.h"
#include "qemu/range.h"
#include "trace.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/hotplug.h"
#include "hw/boards.h"
#include "qapi/error.h"
#include "qemu/cutils.h"
#include "pci-internal.h"
#include "hw/xen/xen.h"
#include "hw/i386/kvm/xen_evtchn.h"

#define TYPE_PCI_DEV_EMU        "pci-dev-emu"
#define EMU_MMIO_REGION_NAME    "pci-emu-dev-mmio"
#define EMU_PRTIO_REGION_NAME   "pci-emu-dev-prtio"
#define EMU_PCI_DEV_DESC        "PCI emulation device"

#define PCI_EMU_DEVICE(obj) \
    OBJECT_CHECK(PCIEmuDevice, (obj), TYPE_PCI_DEV_EMU)

#define EMU_MEMIOSPACE_SIZE (8)
#define EMU_PRTIOSPACE_SIZE (8)

#define CUSTOM_DEVICE_ID    (0xAABB)
#define CUSTOM_PCICLSS_VER  (0x00)

typedef struct PCIEmuDevice {
    PCIDevice pcidev;
    MemoryRegion memiospace;
    MemoryRegion prtiospace;
    uint64_t mmio_val;
    uint64_t prtio_val;
} PCIEmuDevice;

static uint64_t pci_emu_dev_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    PCIEmuDevice *ped = PCI_EMU_DEVICE(opaque);

    if(size >= 4 && size <= 8)
        return ped->mmio_val;

    return 0;
}

static void pci_emu_dev_mmio_write(void *opaque, hwaddr addr, uint64_t val,
        unsigned size)
{
    PCIEmuDevice *prv_dev = PCI_EMU_DEVICE(opaque);
    PCIDevice * pci_dev = PCI_DEVICE(opaque);

    assert(size >= 4 && size <= 8);
    prv_dev->mmio_val = val;
    msi_notify(pci_dev,0);
}

static uint64_t pci_emu_dev_prtio_read(void *opaque, hwaddr addr, unsigned size)
{
    PCIEmuDevice *prv_dev = PCI_EMU_DEVICE(opaque);

    if(size >= 4 && size <= 8)
        return prv_dev->prtio_val;

    return 0;
}

static void pci_emu_dev_prtio_write(void *opaque, hwaddr addr,
    uint64_t val,unsigned size)
{
    PCIEmuDevice *prv_dev = PCI_EMU_DEVICE(opaque);
    PCIDevice * pci_dev = PCI_DEVICE(opaque);

    assert(size >= 4 && size <= 8);
    prv_dev->prtio_val = val;
    msi_notify(pci_dev, 0);
}

static const MemoryRegionOps pci_emu_dev_mmio_ops = {
    .read = pci_emu_dev_mmio_read,
    .write = pci_emu_dev_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4, /* can write nibbles */
        .max_access_size = 8,
    },
};

static const MemoryRegionOps pci_emu_dev_prtio_ops = {
    .read = pci_emu_dev_prtio_read,
    .write = pci_emu_dev_prtio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4, /* can write nibbles */
        .max_access_size = 8,
    },
};

static void pci_emu_dev_realize(PCIDevice *dev, Error **errp)
{
    PCIEmuDevice *prv_dev = PCI_EMU_DEVICE(dev);

    /* init memory regions*/
    memory_region_init_io(&prv_dev->memiospace, OBJECT(prv_dev),
        &pci_emu_dev_mmio_ops, prv_dev,
        EMU_MMIO_REGION_NAME, EMU_MEMIOSPACE_SIZE);
    memory_region_init_io(&prv_dev->prtiospace, OBJECT(prv_dev),
        &pci_emu_dev_prtio_ops, prv_dev,
        EMU_PRTIO_REGION_NAME, EMU_PRTIOSPACE_SIZE);

    /* allocate bar registers */
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &prv_dev->memiospace);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_SPACE_IO, &prv_dev->prtiospace);

    /* Add msi capability
     * offset 0 : let pci qemu core
     * find the first non used ofsset 
     * to inject msi capability
     */
    if(msi_init(dev, 0, 1, true, false, errp))
        return;
}

static void pci_emu_dev_exit(PCIDevice *dev)
{
    msi_uninit(dev);
    return;
}

static void pci_emu_dev_cls_init(ObjectClass *klass, void *data)
{
    DeviceClass *devcls = DEVICE_CLASS(klass);
    PCIDeviceClass *pcicls = PCI_DEVICE_CLASS(klass);

    pcicls->realize = pci_emu_dev_realize;
    pcicls->exit = pci_emu_dev_exit;
    pcicls->vendor_id = PCI_VENDOR_ID_QEMU;
    pcicls->device_id = CUSTOM_DEVICE_ID;
    pcicls->class_id = PCI_CLASS_OTHERS;
    pcicls->revision = CUSTOM_PCICLSS_VER;
    devcls->desc = EMU_PCI_DEV_DESC;
    set_bit(DEVICE_CATEGORY_MISC, devcls->categories);
}

static InterfaceInfo ifs[] = {
    {INTERFACE_CONVENTIONAL_PCI_DEVICE},
    {},
};

static const TypeInfo pci_example_info = {
    .name = TYPE_PCI_DEV_EMU,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIEmuDevice),
    .class_init = pci_emu_dev_cls_init,
    .interfaces = ifs,
};

static void pci_emu_dev_reg_types(void)
{
    type_register_static(&pci_example_info);
}

type_init(pci_emu_dev_reg_types)
