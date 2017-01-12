#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Wetterwald");
MODULE_DESCRIPTION("A learning-purpose naive Realtek 8139D driver implementation");

static struct net_device * r8139dn;

static struct net_device_ops r8139dn_ops =
{
};

// This is the list of devices we claim to be the driver for
static struct pci_device_id r8139dn_pci_id_table[] =
{
    {PCI_DEVICE(PCI_VENDOR_ID_REALTEK, PCI_DEVICE_ID_REALTEK_8139)},
    {0, },
};

// Tell userspace what device this driver is for
MODULE_DEVICE_TABLE(pci, r8139dn_pci_id_table);

// r8139dn_pci_probe is called by the kernel when the device we want
// has been detected somewhere on the PCI Bus.
static int r8139dn_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int err;

    pr_info("Device detected\n");

    // We only support revision 0x10 for now.
    if (pdev->revision != 0x10)
    {
        pr_err("This device (rtl8139 revision %02x) is not supported\n", pdev->revision);
        return -ENODEV;
    }

    // Allocate a eth device
    r8139dn = alloc_etherdev(0);
    if (!r8139dn)
    {
        return -ENOMEM;
    }

    r8139dn->netdev_ops = &r8139dn_ops;

    // Tell the kernel to show our eth interface to userspace (in ifconfig -a)
    err = register_netdev(r8139dn);
    if (err)
    {
        goto free_netdev;
    }

    // Enable our PCI device so that it is woken up
    err = pci_enable_device(pdev);
    if (err)
    {
        goto free_netdev;
    }

    return 0;

free_netdev:
    free_netdev(r8139dn);
    return err;
}

// r8139dn_pci_remove is called whenever the device is removed from the PCI bus.
// This will also be the case if our module is unloaded from the kernel.
static void r8139dn_pci_remove(struct pci_dev *pdev)
{
    pr_info("Device left\n");

    // Disable our PCI device
    pci_disable_device(pdev);

    // Tell the kernel our eth interface doesn't exist anymore (will disappear from ifconfig -a)
    unregister_netdev(r8139dn);

    // Free the structure reprensenting our eth interface
    free_netdev(r8139dn);
}

// r8139dn_pci_driver represents our PCI driver.
// It has several functors so that the kernel knows what to call.
// .id_table is a list of devices our driver claims to be responsible for.
static struct pci_driver r8139dn_pci_driver =
{
    .name = KBUILD_MODNAME,
    .id_table = r8139dn_pci_id_table,
    .probe = r8139dn_pci_probe,
    .remove = r8139dn_pci_remove,
};

// r8139dn_mod_init will be called whenever our module is loaded to kernel memory.
// This will happen no matter if there is a device on the PCI bus or not.
static int __init r8139dn_mod_init(void)
{
    pr_info("Hello!\n");
    // Our module informs the kernel that there is a new PCI driver
    return pci_register_driver(&r8139dn_pci_driver);
}

// r8139dn_mod_init will be called whenever our module is unloaded from kernel memory.
static void __exit r8139dn_mod_exit(void)
{
    // Remove the PCI driver from the kernel list so that we won't be a driver candidate anymore.
    pci_unregister_driver(&r8139dn_pci_driver);
    pr_info("Bye!\n");
}

// Tell the kernel which functions should be called when this module is loaded/unloaded.
module_init(r8139dn_mod_init);
module_exit(r8139dn_mod_exit);
