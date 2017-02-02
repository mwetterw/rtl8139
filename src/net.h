#ifndef _R8139DN_NET_H
#define _R8139DN_NET_H

#include "hw.h"

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

    struct r8139dn_tx_ring
    {
        // Index of the ring's buffers addresses (in CPU virtual kernel memory space)
        void * data [ R8139DN_TX_DESC_NB ];

        // Address hardware has to use in Bus Address Space to access our data buffers above
        dma_addr_t dma;

        // These are the position of the CPU and of the hardware
        // Position of the CPU is the next buffer we are going to write to
        // Position of the hardware is the first un-acknowledged buffer (buffer we cannot write to)
        int cpu, hw;
    } tx_ring;

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
