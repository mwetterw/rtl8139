#include "common.h"
#include "net.h"
#include "hw.h"

#include <linux/module.h>       // MODULE_PARM_DESC
#include <linux/moduleparam.h>  // module_param
#include <linux/interrupt.h>    // IRQF_SHARED, irqreturn_t, request_irq, free_irq

static irqreturn_t r8139dn_net_interrupt ( int irq, void * dev );
static void _r8139dn_net_interrupt_tx ( struct net_device * ndev );
static void _r8139dn_net_interrupt_rx ( struct net_device * ndev );
static void _r8139dn_net_check_link ( struct net_device * ndev );

static int r8139dn_net_open ( struct net_device * ndev );
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev );
static int r8139dn_net_close ( struct net_device * ndev );

static int r8139dn_net_set_mac_addr ( struct net_device * ndev, void * addr );
static int r8139dn_net_set_mtu ( struct net_device * ndev, int mtu );

static int _r8139dn_net_init_tx_ring ( struct r8139dn_priv * priv );
static int _r8139dn_net_init_rx_ring ( struct r8139dn_priv * priv );
static void _r8139dn_net_release_rings ( struct r8139dn_priv * priv );

static int debug = -1;
module_param ( debug, int, 0 );
MODULE_PARM_DESC ( debug, "Debug setting" );

enum { TX = 1, RX = 2, LBK = 4 };
static int txrx = ( TX | RX );
module_param ( txrx, int, 0 );
MODULE_PARM_DESC ( txrx, "TXRX Mode: TX (0x1) | RX (0x2) | Loopback (0x4)" );


// r8139dn_ops stores functors to our driver actions,
// so that the kernel can call the relevant one when needed
static struct net_device_ops r8139dn_ops =
{
    .ndo_open = r8139dn_net_open,
    .ndo_start_xmit = r8139dn_net_start_xmit,
    .ndo_stop = r8139dn_net_close,

    .ndo_set_mac_address = r8139dn_net_set_mac_addr,
    .ndo_change_mtu      = r8139dn_net_set_mtu,
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

    // If one day alloc_etherdev doesn't kzalloc anymore, we may kernel panic
    // This would happen when calling _r8139dn_net_release_rings
    priv = netdev_priv ( ndev );
    priv -> msg_enable = netif_msg_init ( debug, R8139DN_MSG_ENABLE );
    priv -> pdev = pdev;
    priv -> mmio = mmio;

    // Bind our driver functors struct to our net device
    ndev -> netdev_ops = & r8139dn_ops;

    // Add our net device as a leaf to our PCI device in /sys tree
    SET_NETDEV_DEV ( ndev, & ( pdev -> dev ) );

    // Ask the network card to do a soft reset
    err = r8139dn_hw_reset ( priv );
    if ( err )
    {
        goto err_init_hw_reset;
    }

    // Retrieve MAC address from device's EEPROM and tell the kernel
    r8139dn_hw_eeprom_mac_to_kernel ( ndev );

    // Make RX activity also noticeable together with TX on LED0, we have no LED2 :'(
    r8139dn_hw_configure_leds ( priv, CFG1_LEDS_TXRX_LNK_FDX );

    // Tell the kernel to show our eth interface to userspace (in ifconfig -a)
    err = register_netdev ( ndev );
    if ( err )
    {
        goto err_init_register_netdev;
    }

    // From the PCI device, we want to be able to retrieve our network device
    // So we store it. Later we can retrieve it with pci_get_drvdata
    pci_set_drvdata ( pdev, ndev );

    if ( ! ( txrx & ( TX | RX ) ) )
    {
        netdev_warn ( ndev, "Neither TX nor RX is activated. Is this really what you want?\n" );
    }

    if ( netif_msg_probe ( priv ) )
    {
        netdev_info ( ndev, "Ready!\n" );
    }

    return 0;

err_init_hw_reset:
err_init_register_netdev:
    free_netdev ( ndev );
    return err;
}

// The kernel calls this when interface is set up
// ip link set up dev eth0
static int r8139dn_net_open ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    int irq = priv -> pdev -> irq;
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
    ndev -> irq = irq;
    priv -> interrupts = INT_LNKCHG_PUN | INT_TIMEOUT;
    priv -> tcr = TCR_IFG_DEFAULT | TCR_MXDMA_1024;

    // Issue a software reset
    err = r8139dn_hw_reset ( priv );
    if ( err )
    {
        goto err_open_hw_reset;
    }

    // Restore what the kernel thinks our MAC is to our IDR registers
    r8139dn_hw_kernel_mac_to_regs ( ndev );

    // Assume link is down unless proven otherwise
    // Then, make an initial link check to find out
    netif_carrier_off ( ndev );
    _r8139dn_net_check_link ( ndev );

    if ( txrx & TX )
    {
        // Allocate TX DMA and initialize TX ring
        err = _r8139dn_net_init_tx_ring ( priv );
        if ( err )
        {
            goto err_open_init_ring;
        }

        if ( txrx & LBK )
        {
            netdev_info ( ndev, "Enabling Loopback mode\n" );
            priv -> tcr |= TCR_LBK_ENABLE;
        }

        // Enable TX, load default TX settings
        // and inform the hardware where our shared memory is (DMA)
        r8139dn_hw_setup_tx ( priv );

        // Inform the kernel, that right after the completion of this ifup,
        // he can give us packets immediately: we are ready to be his postman!
        netif_start_queue ( ndev );

        priv -> interrupts |= INT_TX;
    }
    else
    {
        // Prevent the kernel from giving us packets as we're not willing to TX
        netif_stop_queue ( ndev );
    }

    if ( txrx & RX )
    {
        // Allocate RX DMA and initialize RX ring
        err = _r8139dn_net_init_rx_ring ( priv );
        if ( err )
        {
            goto err_open_init_ring;
        }

        // Enable RX, load default RX settings and inform hardware where to DMA
        r8139dn_hw_setup_rx ( priv );

        priv -> interrupts |= INT_RX;
    }

    // Enable interrupts so that hardware can notify us about important events
    r8139dn_hw_enable_irq ( priv );

    return 0;

err_open_hw_reset:
err_open_init_ring:
    _r8139dn_net_release_rings ( priv );
    free_irq ( irq, ndev );

    return err;
}

// The kernel gives us a packet to transmit by calling this function
// This function will never run in parallel with itself (thanks to the xmit_lock spinlock)
// We don't need to protect us from ourselves, but care is needed for shared data with TX IRQ handler
static netdev_tx_t r8139dn_net_start_xmit ( struct sk_buff * skb, struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    struct r8139dn_tx_ring * ring = & priv -> tx_ring;
    u32 flags;
    u16 len;
    int cpu, hw;

    // We can safely read our cpu position without any protection
    // Nobody except us (start_xmit) will ever update this variable
    cpu = ring -> cpu;

    // Care must be taken when retrieving the hw position,
    // as TX IRQ handler may change its value at any time on another CPU
    // Make sure we see the last updated value since TX IRQ handler's last store_release
    hw = smp_load_acquire ( & ring -> hw );

    netdev_dbg ( ndev, "TX request! (%d bytes, %d|%d)\n", skb -> len, hw, cpu );

    // This length is Ethernet header + payload, but without the FCS
    len = skb -> len;

    // Drop packets that are too big for us
    if ( len + ETH_FCS_LEN > R8139DN_MAX_ETH_SIZE )
    {
        if ( netif_msg_tx_err ( priv ) )
        {
            netdev_err ( ndev, "TX dropped! (%d bytes is too big for me)\n", len + ETH_FCS_LEN );
        }
        dev_kfree_skb ( skb );
        ndev -> stats.tx_errors++;
        ndev -> stats.tx_dropped++;
        return NETDEV_TX_OK;
    }

    // We need to implement padding if the frame is too short
    // Our hardware doesn't handle this
    if ( len < ETH_ZLEN )
    {
        memset ( ring -> data [ cpu ] + len, 0, ETH_ZLEN - len );
        len = ETH_ZLEN;
    }

    // Copy the packet to the shared memory with the hardware
    // This also adds the CRC FCS (computed by the software)
    skb_copy_and_csum_dev ( skb, ring -> data [ cpu ] );

    // Get rid of the now useless sk_buff :'(
    // Yes, it's the deep down bottom of the TCP/IP stack here :-)
    dev_kfree_skb ( skb );

    // The last missing info in the flags is the length of this frame
    flags = priv -> tx_flags | len;

    // Transmit frame to the world, to __THE INTERNET__!
    r8139dn_w32 ( TSD0 + cpu * TSD_GAP, flags );

    // Move our own position (and modulo it)
    // TX IRQ handler is going to read the cpu pos, be careful when updating it
    // Make sure TX IRQ handler will see the new value upon next load_acquire
    BUILD_BUG_ON_NOT_POWER_OF_2 ( R8139DN_TX_DESC_NB );
    smp_store_release ( & ring -> cpu, ( cpu + 1 ) & ( R8139DN_TX_DESC_NB - 1 ) );

    // If our network card is overwhelmed with packets to transmit
    // We need to tell the kernel to stop giving us packets
    // That way, we don't overwrite packets that haven't been processed yet
    // TX buffer is full when abs(hw - cpu) is 1. Because when 0, it means empty
    if ( ( ( hw - ring -> cpu ) & ( R8139DN_TX_DESC_NB - 1 ) ) == 1 )
    {
        netdev_dbg ( ndev, "  TX ring buffer full, stopping queue\n" );
        netif_stop_queue ( ndev );
    }

    return NETDEV_TX_OK;
}

static irqreturn_t r8139dn_net_interrupt ( int irq, void * dev )
{
    struct net_device * ndev = ( struct net_device * ) dev;
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    u16 isr = r8139dn_r16 ( ISR );

    // Shared IRQ... Return immediately if we have actually nothing to do
    // Tell the kernel our device was not the trigger for this interrupt
    if ( ! isr )
    {
        netdev_dbg ( ndev, "IRQ_NONE\n" );

#ifdef ASM1083_LOST_INTx_DEASSERT_FIX
        // Work arround for ASM1083 PCIe to PCI Bridge
        // Some MSI INTx-Deassert messages are lost or late
        // We manually trigger an interrupt, hoping a new INTx-Dessert message
        // will be generated by the ASM1083 bridge and then seen by the I/O APIC
        // We want an interrupt to be raised very soon
        r8139dn_w32 ( TIMERINT, 1 );
        r8139dn_w32 ( TCTR, 0 );
#endif

        return IRQ_NONE;
    }

    netdev_dbg ( ndev, "IRQ (ISR: %04x)\n", isr );

#ifdef ASM1083_LOST_INTx_DEASSERT_FIX
    // If we are fixing the spurious interrupt
    if ( isr & INT_TIMEOUT && r8139dn_r32 ( TIMERINT ) == 1 )
    {
        r8139dn_w32 ( TIMERINT, 0 ); // Disable the timer interruption
    }
#endif

    // Acknowledge IRQ as fast as possible
    r8139dn_w16 ( ISR, isr );

    // Only care about interrupts we are interested in
    isr &= priv -> interrupts;

    // The link status changed.
    if ( isr & INT_LNKCHG_PUN )
    {
        netdev_dbg ( ndev, "  Link Changed\n" );

        // Let's check and inform the kernel about the change
        _r8139dn_net_check_link ( ndev );
    }

    // We have some RX homework to do!
    if ( isr & INT_RX )
    {
        _r8139dn_net_interrupt_rx ( ndev );
    }

    // We have some TX homework to do :)
    if ( isr & INT_TX )
    {
        _r8139dn_net_interrupt_tx ( ndev );
    }

    return IRQ_HANDLED;
}

// This function does the TX homework during interruption
// It checks the status of each packet in the ring buffer and acknowledges it
static void _r8139dn_net_interrupt_tx ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    struct r8139dn_tx_ring * tx_ring = & priv -> tx_ring;
    int cpu, * hw, hw_old;
    u32 tsd;

    netdev_dbg ( ndev, "  TX homework!\n" );

    // IRQ handler is the only place updating the hw pos
    // No protection is required when retrieving the value
    hw = & tx_ring -> hw;
    hw_old = * hw;

    // Care must be taken when retrieving cpu pos as start_xmit may update it on another CPU
    // Make sure our CPU sees the updated value made by the last store_release in start_xmit
    cpu = smp_load_acquire ( & tx_ring -> cpu );

    // Empty as many buffers as possible in only one interrupt
    // While the TX ring buffer is not empty
    while ( * hw != cpu )
    {
        // Fetch the transmit status of current TX buffer
        // (Network card fills this for us to report TX result for each buffer)
        tsd = r8139dn_r32 ( TSD0 + * hw * TSD_GAP );
        netdev_dbg ( ndev, "    TSD%d: %08x\n", * hw, tsd & ~ ( TSD_ERTXTH | TSD_SIZE ) );

        // Hardware hasn't given any feedback on the transmission of this buffer
        // This means it hasn't been TX yet. It's still in the FIFO, moving to line
        if ( ! ( tsd & ( TSD_TOK | TSD_TUN | TSD_TABT ) ) )
        {
            break;
        }

        if ( tsd & TSD_TOK )
        {
            // Packet has been moved to line successfuly!
            ndev -> stats.tx_packets++;
            ndev -> stats.tx_bytes += ( tsd & TSD_SIZE );
        }
        else
        {
            // There was some TX error, first log it
            if ( netif_msg_tx_err ( priv ) )
            {
                netdev_err ( ndev, "TX error (buf %d): %08x\n", * hw, tsd );
            }

            // We've cleared TER at beginning of IRQ but it might have been set again
            r8139dn_w16 ( ISR, INT_TER );
            ndev -> stats.tx_errors++;

            if ( tsd & TSD_TABT )
            {
                ndev -> stats.tx_aborted_errors++;
            }

            if ( tsd & TSD_TUN )
            {
                ndev -> stats.tx_fifo_errors++;
            }
        }

        // Increment hw position (marks current buffer as free for start_xmit)
        BUILD_BUG_ON_NOT_POWER_OF_2 ( R8139DN_TX_DESC_NB );
        smp_store_release ( hw, ( * hw + 1 ) & ( R8139DN_TX_DESC_NB - 1 ) );
    }

    // If the queue was stopped (buffer full) and we've just freed some space, awake queue!
    // Kernel will resume calling start_xmit callback
    if ( netif_queue_stopped ( ndev ) && * hw != hw_old )
    {
        netdev_dbg ( ndev, "    TX ring buffer has free space, awaking queue\n" );
        netif_wake_queue ( ndev );
    }
}

// This function does the RX homework during interruption
// The NIC retrieves packets from the cable and put them into a buffer.
// We retrieve them from the buffer, create a skbbuf and give them to the kernel.
static void _r8139dn_net_interrupt_rx ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    struct r8139dn_rx_ring * rx_ring = & priv -> rx_ring;
    struct r8139dn_rx_header * rxh;
    struct sk_buff * skb;
    int len, tail_frag_size;
    u16 rx_offset;

    // Let's break the build if the assumptions we heavily rely on are wrong
    BUILD_BUG_ON ( sizeof ( struct r8139dn_rx_header ) != R8139DN_RX_HEADER_SIZE );
    BUILD_BUG_ON_NOT_POWER_OF_2 ( R8139DN_RX_BUFLEN );

    netdev_dbg ( ndev, "  RX homework!\n" );

    // While the RX Buffer is not empty
    while ( ! ( r8139dn_r8 ( CR ) & CR_BUFE ) )
    {
        /*   RTL RX Header          802.3 Ethernet Frame          32 bit Align
         * <---------------><------------------------------------><---------->
         *  -----------------------------------------------------------------
         * | STATUS | SIZE | DMAC | SMAC | LEN/TYPE | DATA | FCS | ALIGNMENT |
         *  -----------------------------------------------------------------
         *     2       2       6      6       2       /~/     4      [0;3]
         */

        // Compute our position in the RX ring buffer
        // Avoid expensive % operator (equivalent to cpu % R8139DN_RX_BUFLEN)
        rx_offset = ( rx_ring -> cpu ) & ( R8139DN_RX_BUFLEN - 1 );

        // Fetch the RX Header to get the status and the size of the frame
        rxh = ( struct r8139dn_rx_header * ) ( rx_ring -> data + rx_offset );

        netdev_dbg ( ndev, "    CBR: %u, CAPR: %u, Offset: %u, Size: %u, Status: 0x%04x\n",
                r8139dn_r16 ( CBR ), r8139dn_r16 ( CAPR ), rx_offset, rxh -> size, rxh -> status );

        // Don't give the Ethernet checksum to the kernel
        len = rxh -> size - ETH_FCS_LEN;

        // Allocate an skbuff and add 2 bytes at the beginning to align for IP header
        skb = netdev_alloc_skb_ip_align ( ndev, len );

        // Copy the Ethernet frame to the skbuff
        if ( skb )
        {
            // The frame spans the end of the buffer, we need to copy the two parts separately
            if ( rx_offset + R8139DN_RX_HEADER_SIZE + len > R8139DN_RX_BUFLEN )
            {
                tail_frag_size = R8139DN_RX_BUFLEN - ( rx_offset + R8139DN_RX_HEADER_SIZE );
                skb_copy_to_linear_data ( skb, rxh + 1, tail_frag_size );
                skb_copy_to_linear_data_offset ( skb, tail_frag_size, rx_ring -> data, len - tail_frag_size );
            }
            // The frame doesn't span the end of the buffer: it is in one piece
            else
            {
                skb_copy_to_linear_data ( skb, rxh + 1, len );
            }

            skb_put ( skb, len );
            skb -> protocol = eth_type_trans ( skb, ndev );

            // Feed the kernel's IP stack with our freshly RXed Ethernet frame!
            netif_rx ( skb );
        }

        // Commit to the hardware our new position in the ring buffer
        rx_ring -> cpu += R8139DN_RX_ALIGN ( rxh -> size + R8139DN_RX_HEADER_SIZE );
        r8139dn_w16 ( CAPR, rx_ring -> cpu - R8139DN_RX_PAD );
    }
}

// The kernel calls this when interface is set down
// ip link set down dev eth0
static int r8139dn_net_close ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );

    if ( netif_msg_ifdown ( priv ) )
    {
        netdev_info ( ndev, "Bringing interface down...\n" );
    }

    if ( txrx & ( TX | RX ) )
    {
        // Disable TX and RX
        r8139dn_hw_disable_transceiver ( priv );
    }

    // Disable IRQ
    r8139dn_hw_disable_irq ( priv );

    // Free all allocated DMA memory
    _r8139dn_net_release_rings ( priv );

    // Unhook our handler from the IRQ line
    free_irq ( ndev -> irq, ndev );

    return 0;
}

static void _r8139dn_net_check_link ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    // Fetch the Media Status Register
    u8 msr = r8139dn_r8 ( MSR );

    // If the link was down but is now up, tell the kernel
    if ( ! netif_carrier_ok ( ndev ) && ! ( msr & MSR_LINK_BAD ) )
    {
        if ( netif_msg_link ( priv ) )
        {
            netdev_info ( ndev, "Link is now up!\n" );
        }
        netif_carrier_on ( ndev );
    }

    // If the link was up but is now down, tell the kernel
    else if ( netif_carrier_ok ( ndev ) && ( msr & MSR_LINK_BAD ) )
    {
        if ( netif_msg_link ( priv ) )
        {
            netdev_info ( ndev, "Link is now down!\n" );
        }
        netif_carrier_off ( ndev );
    }
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

// Called when the user wants to change the MTU
// ip link set mtu 1500 dev eth0
static int r8139dn_net_set_mtu ( struct net_device * ndev, int mtu )
{
    // Very low MTU is allowed because we'll proceed with padding in start_xmit anyway
    if ( mtu > R8139DN_MAX_MTU || mtu <= 0 )
    {
        return -EINVAL;
    }

    ndev -> mtu = mtu;
    return 0;
}

// Allocate TX DMA memory and initialize TX ring
static int _r8139dn_net_init_tx_ring ( struct r8139dn_priv * priv )
{
    void * tx_buffer_cpu;
    dma_addr_t tx_buffer_dma;
    int i;

    // Allocate a DMA buffer so that the hardware and the driver
    // share a common memory for packet transmission.
    // Later we will pass the tx_buffer_dma address to the hardware
    tx_buffer_cpu = dma_alloc_coherent ( & ( priv -> pdev -> dev ),
            R8139DN_TX_DMA_SIZE, & tx_buffer_dma, GFP_KERNEL );

    if ( ! tx_buffer_cpu )
    {
        return -ENOMEM;
    }

    // Initialize the TX ring
    // cpu and hw fields are reset by r8139dn_hw_reset
    priv -> tx_ring.dma = tx_buffer_dma;
    for ( i = 0; i < R8139DN_TX_DESC_NB ; ++i )
    {
        // Initialize our index of TX buffers addresses
        priv -> tx_ring.data [ i ] = tx_buffer_cpu + i * R8139DN_TX_DESC_SIZE;
    }

    return 0;
}

// Allocate RX DMA memory and initialize RX ring
static int _r8139dn_net_init_rx_ring ( struct r8139dn_priv * priv )
{
    void * rx_buffer_cpu;
    dma_addr_t rx_buffer_dma;

    // Allocate a DMA buffer so that hardware and driver share a common memory
    // for packet reception. Later we'll pass the rx_buffer_dma address to the hardware
    rx_buffer_cpu = dma_alloc_coherent ( & ( priv -> pdev -> dev ),
            R8139DN_RX_DMA_SIZE, & rx_buffer_dma, GFP_KERNEL );

    if ( ! rx_buffer_cpu )
    {
        return -ENOMEM;
    }

    priv -> rx_ring.dma = rx_buffer_dma;
    priv -> rx_ring.data = rx_buffer_cpu;

    // When alloc_etherdev is called, our priv data struct is zeroed (kzalloc)
    // However we still need to reset this field (multiple ifup/ifdown)
    priv -> rx_ring.cpu = 0;

    return 0;
}

// Free all allocated DMA Memory (TX/RX)
static void _r8139dn_net_release_rings ( struct r8139dn_priv * priv )
{
    // Free TX DMA memory
    if ( priv -> tx_ring.data [ 0 ] )
    {
        dma_free_coherent ( & ( priv -> pdev -> dev ), R8139DN_TX_DMA_SIZE,
                priv -> tx_ring.data [ 0 ], priv -> tx_ring.dma );
    }

    // Free RX DMA Memory
    if ( priv -> rx_ring.data )
    {
        dma_free_coherent ( & ( priv -> pdev -> dev ), R8139DN_RX_DMA_SIZE,
                priv -> rx_ring.data, priv -> rx_ring.dma );
    }
}
