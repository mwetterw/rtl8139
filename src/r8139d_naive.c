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

static struct pci_device_id r8139dn_pci_id_table[] =
{
    {PCI_DEVICE(PCI_VENDOR_ID_REALTEK, PCI_DEVICE_ID_REALTEK_8139)},
    {0, },
};

MODULE_DEVICE_TABLE(pci, r8139dn_pci_id_table);

static int r8139dn_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int err;

    pr_info("Device detected\n");
    if (pdev->revision != 0x10)
    {
        pr_err("This device (rtl8139 revision %02x) is not supported\n", pdev->revision);
        return -ENODEV;
    }

    r8139dn = alloc_etherdev(0);
    if (!r8139dn)
    {
        return -ENOMEM;
    }

    r8139dn->netdev_ops = &r8139dn_ops;

    err = register_netdev(r8139dn);
    if (err)
    {
        goto free_netdev;
    }

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

static void r8139dn_pci_remove(struct pci_dev *pdev)
{
    pr_info("Device left\n");
    pci_disable_device(pdev);
    unregister_netdev(r8139dn);
    free_netdev(r8139dn);
}

static struct pci_driver r8139dn_pci_driver =
{
    .name = KBUILD_MODNAME,
    .id_table = r8139dn_pci_id_table,
    .probe = r8139dn_pci_probe,
    .remove = r8139dn_pci_remove,
};

static int __init r8139dn_mod_init(void)
{
    pr_info("Hello!\n");
    return pci_register_driver(&r8139dn_pci_driver);
}

static void __exit r8139dn_mod_exit(void)
{
    pci_unregister_driver(&r8139dn_pci_driver);
    pr_info("Bye!\n");
}

module_init(r8139dn_mod_init);
module_exit(r8139dn_mod_exit);
