#include "net.h"
#include "hw.h"

static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev );

// r8139dn_ops stores fonctors to our driver actions,
// so that the kernel can call the relevant one when needed
static struct net_device_ops r8139dn_ops =
{
    .ndo_start_xmit = r8139dn_net_start_xmit,
};

int r8139dn_net_init ( struct pci_dev * pdev, void __iomem * mmio )
{
    struct net_device * ndev;
    struct r8139dn_priv * priv;
    int err;

    // Allocate a eth device
    ndev = alloc_etherdev ( sizeof ( * priv ) );
    if ( ! ndev )
    {
        return -ENOMEM;
    }

    priv = netdev_priv ( ndev );
    priv -> pdev = pdev;
    priv -> mmio = mmio;

    // Bind our driver fonctors struct to our net device
    ndev -> netdev_ops = & r8139dn_ops;

    // Add our net device as a leaf to our PCI bus in /sys tree
    SET_NETDEV_DEV ( ndev, & ( pdev -> dev ) );

    // Ask the network card to do a soft reset
    r8139dn_hw_reset ( priv );

    // Retrieve MAC from device and tell the kernel
    r8139dn_hw_mac_load_to_kernel ( ndev );

    // Tell the kernel to show our eth interface to userspace (in ifconfig -a)
    err = register_netdev ( ndev );
    if ( err )
    {
        free_netdev ( ndev );
        return err;
    }

    // From the PCI device, we want to be able to retrieve our network device
    // So we store it. Later we can retrieve it with pci_get_drvdata
    pci_set_drvdata ( pdev, ndev );

    // Forbid the kernel to give us packets to transmit for now...
    netif_stop_queue ( ndev ); // XXX

    return 0;
}

// The kernel gives us a packet to transmit by calling this function
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev )
{
    // TODO
    return NETDEV_TX_BUSY;
}
