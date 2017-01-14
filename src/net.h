#include <linux/netdevice.h>
#include <linux/etherdevice.h>

extern struct net_device_ops r8139dn_ops;

// r8139dn_priv is a struct we can always fetch from the network device
// We can store anything that makes our life easier.
struct r8139dn_priv
{
    struct pci_dev * pdev;
};

struct net_device * r8139dn_net_init ( struct pci_dev * pdev );
