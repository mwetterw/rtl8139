#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Wetterwald");
MODULE_DESCRIPTION("A learning-purpose naive Realtek 8139D driver implementation");

static struct net_device * r8139dn;

static struct net_device_ops r8139dn_ops =
{
};

static int __init r8139dn_mod_init(void)
{
    printk(KERN_INFO "rtl8139d naive driver, Hello!\n");
    r8139dn = alloc_etherdev(0);
    if (!r8139dn)
    {
        return -ENOMEM;
    }

    r8139dn -> netdev_ops = &r8139dn_ops;
    register_netdev(r8139dn);

    return 0;
}

static void __exit r8139dn_mod_exit(void)
{
    printk(KERN_INFO "rtl8139d naive driver, Bye!\n");
    unregister_netdev(r8139dn);
    free_netdev(r8139dn);
}

module_init(r8139dn_mod_init);
module_exit(r8139dn_mod_exit);
