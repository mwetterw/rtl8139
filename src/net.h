#ifndef _R8139DN_NET_H
#define _R8139DN_NET_H

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>

// r8139dn_priv is a struct we can always fetch from the network device
// We can store anything that makes our life easier.
struct r8139dn_priv
{
    int msg_enable;
    struct pci_dev * pdev;
    void __iomem * mmio;

    void * tx_buffer_cpu;
    dma_addr_t tx_buffer_dma;
    int tx_buffer_our_pos;
    int tx_buffer_hw_pos;
    u32 tx_flags;
};

int r8139dn_net_init ( struct pci_dev * pdev, void __iomem * mmio );

#define R8139DN_MSG_ENABLE \
    (NETIF_MSG_DRV       | \
     NETIF_MSG_PROBE     | \
     NETIF_MSG_LINK      | \
     NETIF_MSG_TIMER     | \
     NETIF_MSG_IFDOWN    | \
     NETIF_MSG_IFUP      | \
     NETIF_MSG_RX_ERR    | \
     NETIF_MSG_TX_ERR)

#endif
