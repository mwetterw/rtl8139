#include "net.h"
#include "hw.h"

static int r8139dn_net_open ( struct net_device * ndev );
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev );
static int r8139dn_net_close ( struct net_device * ndev );

// r8139dn_ops stores functors to our driver actions,
// so that the kernel can call the relevant one when needed
static struct net_device_ops r8139dn_ops =
{
    .ndo_open = r8139dn_net_open,
    .ndo_start_xmit = r8139dn_net_start_xmit,
    .ndo_stop = r8139dn_net_close,
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

    // Bind our driver functors struct to our net device
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

    return 0;
}

// The kernel calls this when interface is set up
// ip link set up dev eth0
static int r8139dn_net_open ( struct net_device * ndev )
{
    struct r8139dn_priv * priv;
    void * tx_buffer_cpu;
    dma_addr_t tx_buffer_dma;

    pr_info ( "Bringing interface up...\n" );

    priv = netdev_priv ( ndev );

    // Allocate a DMA buffer so that the hardware and the driver
    // share a common memory for packet transmission.
    // Later we will pass the tx_buffer_dma address to the hardware
    tx_buffer_cpu = dma_alloc_coherent ( & ( priv -> pdev -> dev ),
            R8139DN_TX_DMA_SIZE, & tx_buffer_dma, GFP_KERNEL );

    if ( tx_buffer_cpu == NULL )
    {
        return -ENOMEM;
    }

    priv -> tx_buffer_cpu = tx_buffer_cpu;
    priv -> tx_buffer_dma = tx_buffer_dma;

    // Enable TX, load default TX settings
    // and inform the hardware where our shared memory is (DMA)
    r8139dn_hw_setup_tx ( priv );

    // Inform the kernel, that right after the completion of this ifup,
    // he can give us packets immediately: we are ready to be his postman!
    netif_start_queue ( ndev );

    return 0;
}

// The kernel gives us a packet to transmit by calling this function
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev )
{
    struct r8139dn_priv * priv;
    u32 flags;
    u16 len;

    pr_info ( "TX request! (%d bytes)\n", skb -> len );

    priv = netdev_priv ( ndev );
    len = skb -> len;

    // We need to implement padding if the frame is too short
    // Our hardware doesn't handle this
    if ( len < ETH_ZLEN )
    {
        memset ( ( priv -> tx_buffer_cpu ) + len, 0, ETH_ZLEN - len );
        len = ETH_ZLEN;
    }
    // XXX Add a check for frames that are too long

    // Copy the packet to the shared memory with the hardware
    // This also adds the CRC FCS (computed by the software)
    skb_copy_and_csum_dev ( skb, priv -> tx_buffer_cpu );

    // Get rid of the now useless sk_buff :'(
    // Yes, it's the deep down bottom of the TCP/IP stack here :-)
    dev_kfree_skb ( skb );

    // The last missing info in the flags is the length of this frame
    flags = priv -> tx_flags | len;

    // Transmit frame to the world, to __THE INTERNET__!
    r8139dn_w32 ( TSD0, flags );

    // Forbid the kernel to give us packets to transmit for now...
    // TODO: Be able to send more than only one single packet? :D
    netif_stop_queue ( ndev ); // XXX

    return NETDEV_TX_OK;
}

// The kernel calls this when interface is set down
// ip link set down dev eth0
static int r8139dn_net_close ( struct net_device * ndev )
{
    struct r8139dn_priv * priv;

    pr_info ( "Bringing interface down...\n" );

    priv = netdev_priv ( ndev );

    // Disable TX and RX
    r8139dn_hw_disable_transceiver ( priv );

    // Free TX DMA memory
    dma_free_coherent ( & ( priv -> pdev -> dev ), R8139DN_TX_DMA_SIZE,
            priv -> tx_buffer_cpu, priv -> tx_buffer_dma );

    return 0;
}
