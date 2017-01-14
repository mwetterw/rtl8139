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
    int err;
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

    err = r8139dn_net_init ( &ndev, pdev );
    if ( err )
    {
        goto err_init;
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

err_register:
    free_netdev ( ndev );
err_init:
    __r8139dn_pci_disable ( pdev );
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

    // Free the structure reprensenting our eth interface
    free_netdev ( ndev );

    __r8139dn_pci_disable ( pdev );
}
