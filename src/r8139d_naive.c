#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Wetterwald");
MODULE_DESCRIPTION("A learning-purpose naive Realtek 8139D driver implementation");

static int __init r8139dn_mod_init(void)
{
    printk(KERN_INFO "rtl8139d naive driver, Hello!\n");
    return 0;
}

static void __exit r8139dn_mod_exit(void)
{
    printk(KERN_INFO "rtl8139d naive driver, Bye!\n");
}

module_init(r8139dn_mod_init);
module_exit(r8139dn_mod_exit);
