#include "pci.h"
#include "net.h"

#include <linux/module.h>

// This is the list of devices we claim to be the driver for
struct pci_device_id r8139dn_pci_id_table [ ] =
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

static void __r8139dn_pci_disable ( struct pci_dev * pdev )
{
    // Disable our PCI device
    pci_set_power_state ( pdev, PCI_D3cold );
    pci_disable_device ( pdev );
}

// r8139dn_pci_probe is called by the kernel when the device we want
// has been detected somewhere on the PCI Bus.
int r8139dn_pci_probe ( struct pci_dev * pdev, const struct pci_device_id * id )
{
    int err, len;
    struct net_device * ndev;

    pr_info ( "Device detected\n" );

    // We only support revision 0x10 for now.
    if ( pdev -> revision != 0x10 )
    {
        pr_err ( "This device (rtl8139 revision %02x) is not supported\n", pdev -> revision );
        return -ENODEV;
    }

    // Enable our PCI device so that it is woken up
    err = pci_enable_device ( pdev );
    if ( err )
    {
        return err;
    }

    ndev = r8139dn_net_init ( pdev );
    if ( IS_ERR ( ndev ) )
    {
        err = PTR_ERR ( ndev );
        goto err_init;
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
        pr_err ( "Insufficient region size. Minimum required: %do, got %do.\n", R8139DN_IO_SIZE, len );
        err = -ENODEV;
        goto err_resource;
    }

    // Tell the kernel to show our eth interface to userspace (in ifconfig -a)
    err = register_netdev ( ndev );
    if ( err )
    {
        goto err_register;
    }

    // Enable DMA by setting master bit in PCI_COMMAND register
    pci_set_master ( pdev );

    return 0;

err_register: free_netdev ( ndev );
err_resource: pci_release_regions ( pdev );
err_init:     __r8139dn_pci_disable ( pdev );
    return err;
}

// r8139dn_pci_remove is called whenever the device is removed from the PCI bus.
// This will also be the case if our module is unloaded from the kernel.
void r8139dn_pci_remove ( struct pci_dev * pdev )
{
    struct net_device * ndev;

    pr_info ( "Device left\n" );

    // Retrieve the network device from the PCI device
    ndev = pci_get_drvdata ( pdev );

    // Disable DMA by clearing master bit in PCI_COMMAND register
    pci_clear_master ( pdev );

    // Tell the kernel our eth interface doesn't exist anymore (will disappear from ifconfig -a)
    unregister_netdev ( ndev );

    // Release ownership of PCI regions (BAR0 -> BAR1)
    // Our entry in /proc/iomem and /proc/ioports will disappear
    pci_release_regions ( pdev );

    // Free the structure representing our eth interface
    free_netdev ( ndev );

    __r8139dn_pci_disable ( pdev );
}
