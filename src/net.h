#ifndef _R8139DN_NET_H
#define _R8139DN_NET_H

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>

// r8139dn_priv is a struct we can always fetch from the network device
// We can store anything that makes our life easier.
struct r8139dn_priv
{
    struct pci_dev * pdev;
    void __iomem * mmio;
    void * tx_buffer;
    dma_addr_t tx_buffer_dma;
};

int r8139dn_net_init ( struct pci_dev * pdev, void __iomem * mmio );

#endif
