#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "net.h"
#include "hw.h"

#include <linux/module.h>       // MODULE_PARM_DESC
#include <linux/moduleparam.h>  // module_param
#include <linux/interrupt.h>    // IRQF_SHARED, irqreturn_t, request_irq, free_irq

static irqreturn_t r8139dn_net_interrupt ( int irq, void * dev );

static int r8139dn_net_open ( struct net_device * ndev );
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev );
static struct rtnl_link_stats64 * r8139dn_net_fill_stats ( struct net_device * ndev, struct rtnl_link_stats64 * stats );
static int r8139dn_net_set_mac_addr ( struct net_device * ndev, void * addr );
static int r8139dn_net_close ( struct net_device * ndev );

static int debug = -1;
module_param ( debug, int, 0 );
MODULE_PARM_DESC ( debug, "Debug setting" );

// r8139dn_ops stores functors to our driver actions,
// so that the kernel can call the relevant one when needed
static struct net_device_ops r8139dn_ops =
{
    .ndo_open = r8139dn_net_open,
    .ndo_start_xmit = r8139dn_net_start_xmit,
    .ndo_get_stats64 = r8139dn_net_fill_stats,
    .ndo_set_mac_address = r8139dn_net_set_mac_addr,
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
    priv -> msg_enable = netif_msg_init ( debug, R8139DN_MSG_ENABLE );
    priv -> pdev = pdev;
    priv -> mmio = mmio;

    // Bind our driver functors struct to our net device
    ndev -> netdev_ops = & r8139dn_ops;

    // Add our net device as a leaf to our PCI bus in /sys tree
    SET_NETDEV_DEV ( ndev, & ( pdev -> dev ) );

    // Ask the network card to do a soft reset
    r8139dn_hw_reset ( priv );

    // Retrieve MAC address from device's EEPROM and tell the kernel
    r8139dn_hw_eeprom_mac_to_kernel ( ndev );

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
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    int irq = priv -> pdev -> irq;
    void * tx_buffer_cpu;
    dma_addr_t tx_buffer_dma;
    int err;

    if ( netif_msg_ifup ( priv ) )
    {
        netdev_info ( ndev, "Bringing interface up...\n" );
    }

    // Reserve an shared IRQ line and hook our handler on it
    err = request_irq ( irq, r8139dn_net_interrupt, IRQF_SHARED, ndev -> name, ndev );
    if ( err )
    {
        return err;
    }

    // Allocate a DMA buffer so that the hardware and the driver
    // share a common memory for packet transmission.
    // Later we will pass the tx_buffer_dma address to the hardware
    tx_buffer_cpu = dma_alloc_coherent ( & ( priv -> pdev -> dev ),
            R8139DN_TX_DMA_SIZE, & tx_buffer_dma, GFP_KERNEL );

    if ( ! tx_buffer_cpu )
    {
        free_irq ( irq, ndev );
        return -ENOMEM;
    }

    priv -> tx_buffer_cpu = tx_buffer_cpu;
    priv -> tx_buffer_dma = tx_buffer_dma;

    r8139dn_hw_reset ( priv );

    // Restore what the kernel thinks our MAC is to our IDR registers
    r8139dn_hw_kernel_mac_to_regs ( ndev );

    // Enable TX, load default TX settings
    // and inform the hardware where our shared memory is (DMA)
    r8139dn_hw_setup_tx ( priv );

    // Inform the kernel, that right after the completion of this ifup,
    // he can give us packets immediately: we are ready to be his postman!
    netif_start_queue ( ndev );

    // Enable interrupts so that hardware can notify us about important events
    r8139dn_hw_enable_irq ( priv );

    return 0;
}

// The kernel gives us a packet to transmit by calling this function
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    void * descriptor;
    u32 flags;
    u16 len;

    netdev_dbg ( ndev, "TX request! (%d bytes)\n", skb -> len );

    len = skb -> len;

    // Drop packets that are too big for us
    if ( len + ETH_FCS_LEN > R8139DN_MAX_ETH_SIZE )
    {
        if ( netif_msg_tx_err ( priv ) )
        {
            netdev_dbg ( ndev, "TX dropped! (%d bytes is too big for me)\n", len + ETH_FCS_LEN );
        }
        dev_kfree_skb ( skb );
        // TODO: Update stats
        return NETDEV_TX_OK;
    }

    descriptor =
        priv -> tx_buffer_cpu + priv -> tx_buffer_our_pos * R8139DN_TX_DESC_SIZE;

    // We need to implement padding if the frame is too short
    // Our hardware doesn't handle this
    if ( len < ETH_ZLEN )
    {
        memset ( descriptor + len, 0, ETH_ZLEN - len );
        len = ETH_ZLEN;
    }

    // Copy the packet to the shared memory with the hardware
    // This also adds the CRC FCS (computed by the software)
    skb_copy_and_csum_dev ( skb, descriptor );

    // Get rid of the now useless sk_buff :'(
    // Yes, it's the deep down bottom of the TCP/IP stack here :-)
    dev_kfree_skb ( skb );

    // The last missing info in the flags is the length of this frame
    flags = priv -> tx_flags | len;

    // Transmit frame to the world, to __THE INTERNET__!
    r8139dn_w32 ( TSD0 + priv -> tx_buffer_our_pos * TSD_GAP, flags );

    // Move our own position
    priv -> tx_buffer_our_pos =
        ( priv -> tx_buffer_our_pos + 1 ) % R8139DN_TX_DESC_NB;

    // If our network card is overwhelmed with packets to transmit
    // We need to tell the kernel to stop giving us packets
    // That way, we don't overwrite packets that haven't been processed yet
    if ( priv -> tx_buffer_our_pos == priv -> tx_buffer_hw_pos )
    {
        netdev_dbg ( ndev, "TX buffers full, stopping queue\n" );
        netif_stop_queue ( ndev );
    }

    return NETDEV_TX_OK;
}

static irqreturn_t r8139dn_net_interrupt ( int irq, void * dev )
{
    struct net_device * ndev = ( struct net_device * ) dev;
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    u16 isr = r8139dn_r16 ( ISR );

    if ( netif_msg_intr ( priv ) )
    {
        netdev_dbg ( ndev, "IRQ (ISR: %04x)\n", isr );
    }

    // Shared IRQ... Return immediately if we have actually nothing to do
    if ( ! isr )
    {
        // Tell the kernel our device was not the trigger for this interrupt
        return IRQ_NONE;
    }

    // Clear interrupts so that they don't fire again
    r8139dn_hw_clear_irq ( priv );

    return IRQ_HANDLED;
}

// Called when someone requests our stats
static struct rtnl_link_stats64 * r8139dn_net_fill_stats ( struct net_device * ndev, struct rtnl_link_stats64 * stats )
{
    // TODO
    return stats;
}

// Called when user wants to change the MAC address
// ip link set address 05:04:03:02:01:00 dev eth0
static int r8139dn_net_set_mac_addr ( struct net_device * ndev, void * addr )
{
    struct sockaddr * mac_sa = addr;

    // If the MAC address is not valid, just stop here
    if ( ! is_valid_ether_addr ( mac_sa -> sa_data ) )
    {
        return -EADDRNOTAVAIL;
    }

    // Copy the desired MAC address to kernel (will update what kernel thinks our MAC is)
    memcpy ( ndev -> dev_addr, mac_sa -> sa_data, ETH_ALEN );

    // Really update what the network card thinks its MAC address is
    r8139dn_hw_kernel_mac_to_regs ( ndev );

    return 0;
}

// The kernel calls this when interface is set down
// ip link set down dev eth0
static int r8139dn_net_close ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    int irq = priv -> pdev -> irq;

    if ( netif_msg_ifdown ( priv ) )
    {
        netdev_info ( ndev, "Bringing interface down...\n" );
    }

    // Disable TX and RX
    r8139dn_hw_disable_transceiver ( priv );

    // Disable IRQ
    r8139dn_hw_disable_irq ( priv );

    // Free TX DMA memory
    dma_free_coherent ( & ( priv -> pdev -> dev ), R8139DN_TX_DMA_SIZE,
            priv -> tx_buffer_cpu, priv -> tx_buffer_dma );

    // Unhook our handler from the IRQ line
    free_irq ( irq, ndev );

    return 0;
}
