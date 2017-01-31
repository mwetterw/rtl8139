#include "common.h"
#include "pci.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE ( "GPL" );
MODULE_AUTHOR ( "Martin Wetterwald" );
MODULE_DESCRIPTION ( "A learning-purpose naive Realtek 8139D driver implementation" );

// r8139dn_mod_init will be called whenever our module is loaded to kernel memory.
// This will happen no matter if there is a device on the PCI bus or not.
static int __init r8139dn_mod_init ( void )
{
    pr_info ( "Hello!\n" );
    // Our module informs the kernel that there is a new PCI driver
    return pci_register_driver ( & r8139dn_pci_driver );
}

// r8139dn_mod_exit will be called whenever our module is unloaded from kernel memory.
static void __exit r8139dn_mod_exit ( void )
{
    // Remove the PCI driver from the kernel list so that we won't be a driver candidate anymore.
    pci_unregister_driver ( & r8139dn_pci_driver );
    pr_info ( "Bye!\n" );
}

// Tell the kernel which functions should be called when this module is loaded/unloaded.
module_init ( r8139dn_mod_init );
module_exit ( r8139dn_mod_exit );
