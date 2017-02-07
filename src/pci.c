#include "common.h"
#include "pci.h"
#include "net.h"
#include "hw.h"

#include <linux/module.h>

// This is the list of devices we claim to be the driver for
static struct pci_device_id r8139dn_pci_id_table [ ] =
{
    { PCI_DEVICE ( PCI_VENDOR_ID_REALTEK, PCI_DEVICE_ID_REALTEK_8139 ) },
    { 0, },
};

// Tell userspace what device this driver is for
MODULE_DEVICE_TABLE ( pci, r8139dn_pci_id_table );

// r8139dn_pci_driver represents our PCI driver.
// It has several functors so that the kernel knows what to call.
// .id_table is a list of devices our driver claims to be responsible for.
struct pci_driver r8139dn_pci_driver =
{
    .name = KBUILD_MODNAME,
    .id_table = r8139dn_pci_id_table,
    .probe = r8139dn_pci_probe,
    .remove = r8139dn_pci_remove,
};

// r8139dn_pci_probe is called by the kernel when the device we want
// has been detected somewhere on the PCI Bus.
int r8139dn_pci_probe ( struct pci_dev * pdev, const struct pci_device_id * id )
{
    int err;
    u32 version;
    unsigned int len;
    void __iomem * mmio;
    struct device * dev = & pdev -> dev;

    dev_dbg ( dev, "PCI device is being probed\n" );

    // Enable our PCI device so that it is woken up
    err = pci_enable_device ( pdev );
    if ( err )
    {
        return err;
    }

    // Mark PCI regions (BAR0 -> BAR1) as belonging to us
    // An entry in /proc/iomem and /proc/ioports will appear
    // BAR0 is IOAR and BAR1 is MEMAR
    err = pci_request_regions ( pdev, KBUILD_MODNAME );
    if ( err )
    {
        goto err_init;
    }

    // We need to ensure the region given is big enough for our device
    len = pci_resource_len ( pdev, R8139DN_MEMAR );
    if ( len < R8139DN_IO_SIZE )
    {
        dev_err ( dev, "Insufficient region size. Minimum required: %do, got %do.\n",
                R8139DN_IO_SIZE, len );
        err = -EIO;
        goto err_resource;
    }

    // We want to make sure the region we believe is MEMAR really is.
    if ( ! ( pci_resource_flags ( pdev, R8139DN_MEMAR ) & IORESOURCE_MEM ) )
    {
        dev_err ( dev, "Invalid region type. This should be a MMIO region.\n" );
        err = -EIO;
        goto err_resource;
    }

    // It's finally the time to map the device memory to our virtual memory space! :)
    mmio = pci_iomap ( pdev, R8139DN_MEMAR, len );
    if ( ! mmio )
    {
        dev_err ( dev, "Unable to map the MMIO.\n" );
        err = -EIO;
        goto err_resource;
    }

    // Get the chipset version, display it, and cancel the probe if we don't support it
    version = ioread32 ( mmio + TCR ) & TCR_HWVERID_MASK;
    dev_info ( dev, "Chipset detected: %s (rev %x)\n", r8139dn_hw_version_str ( version ), pdev -> revision );
    if ( version != RTL8100B_8139D )
    {
        dev_err ( dev, "Sorry, this chipset is not supported yet. :(\n" );
        err = -ENODEV;
        goto err_chip_not_supported;
    }

    // Enable DMA by setting master bit in PCI_COMMAND register
    pci_set_master ( pdev );

    // Initialize and register our network interface :)
    err = r8139dn_net_init ( pdev, mmio );
    if ( err )
    {
        goto err_register;
    }

    return 0;

err_register:
    pci_clear_master ( pdev );
err_chip_not_supported:
    pci_iounmap ( pdev, mmio );
err_resource:
    pci_release_regions ( pdev );
err_init:
    pci_disable_device ( pdev );
    return err;
}

// r8139dn_pci_remove is called whenever the device is removed from the PCI bus.
// This will also be the case if our module is unloaded from the kernel.
void r8139dn_pci_remove ( struct pci_dev * pdev )
{
    struct device * dev = & pdev -> dev;
    struct net_device * ndev;
    struct r8139dn_priv * priv;

    // Retrieve the network device from the PCI device
    ndev = pci_get_drvdata ( pdev );

    // Retrieve our private data structure from the network device
    priv = netdev_priv ( ndev );

    // Tell the kernel our eth interface doesn't exist anymore (will disappear from ifconfig -a)
    unregister_netdev ( ndev );

    // Disable DMA by clearing master bit in PCI_COMMAND register
    pci_clear_master ( pdev );

    // Unmap our virtual memory from the device's MMIO memory
    pci_iounmap ( pdev, priv -> mmio );

    // Release ownership of PCI regions (BAR0 -> BAR1)
    // Our entry in /proc/iomem and /proc/ioports will disappear
    pci_release_regions ( pdev );

    if ( netif_msg_probe ( priv ) )
    {
        dev_info ( dev, "PCI device removed\n" );
    }

    // Free the structure representing our eth interface
    free_netdev ( ndev );

    // Signal to the system that we don't use this PCI device anymore
    pci_disable_device ( pdev );
}
