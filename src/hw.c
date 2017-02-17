#include "common.h"
#include "hw.h"
#include "net.h"

static u16 _r8139dn_hw_eeprom_read ( struct r8139dn_priv * priv, u8 word_addr );

// Ask the hardware to reset
// This will disable TX and RX, reset FIFOs,
// reset TX buffer at TSAD0, and set BUFE (RX buffer is empty)
// Note: IDR0 -> 5 and MAR0 -> 5 are not reset
int r8139dn_hw_reset ( struct r8139dn_priv * priv )
{
    int i = 1000;

    // Ask the chip to reset
    r8139dn_w8 ( CR, CR_RST );

    // Wait until the reset is complete or timeout
    // The chip notify us by clearing the bit
    while ( --i )
    {
        if ( ! ( r8139dn_r8 ( CR ) & CR_RST ) )
        {
            // Reset is complete!
            break;
        }
        udelay ( 1 );
    }

    if ( ! i )
    {
        return -ETIMEDOUT;
    }

    // Resetting the chip also resets hardware TX pointer to TSAD0
    // So we need to keep track of this, and we also reset our own position
    priv -> tx_ring.hw = 0;
    priv -> tx_ring.cpu = 0;

    return 0;
}

// Read a word (16 bits) from the EEPROM at word_addr address
// We could actually also use <linux/eeprom_93cx6.h> :)
// EEPROM content is in the Little Endian fashion
// But I make sure I return to you the value in your native CPU byte-order
static u16 _r8139dn_hw_eeprom_read ( struct r8139dn_priv * priv, u8 word_addr )
{
    // The command is an 3 bits opcode (EE_READ) followed by 6 bits address
    u16 cmd = ( EE_CMD_READ << EE_ADDRLEN ) | word_addr;
    u8 flags = EE_CR_PROGRAM | EE_CR_EECS; // We'll keep those flags high until the end
    __le16 res = 0;
    u8 eedi;
    bool bit;
    int i;

    // Enable EEPROM Programming mode so that we can access EEPROM pins
    // In following commands, keep Programming mode enabled to maintain access to the pins
    r8139dn_w8 ( EE_CR, EE_CR_PROGRAM );

    // Just send a Chip Select and then keep it high until the end
    // This tells the EEPROM we are talking to her and not to someone else sharing the bus :)
    r8139dn_w8 ( EE_CR, flags );
    udelay ( 1 );

    // Send our read command to the EEPROM
    // We have only 9 bits to send (3 + 6 as stated above), so from 8 to 0
    for ( i = EE_CMD_READ_LEN - 1 ; i >= 0 ; --i )
    {
        // If the bit we want to send is 1, we assert EEDI pin
        eedi = cmd & ( 1 << i ) ? EE_CR_EEDI : 0;
        r8139dn_w8 ( EE_CR, flags | eedi ); // Clock low with our data
        udelay ( 1 );
        r8139dn_w8 ( EE_CR, flags | EE_CR_EESK | eedi ); // Clock high with our data
        udelay ( 1 );
    }

    r8139dn_w8 ( EE_CR, flags ); // Clock low
    udelay ( 1 );

    // Fetch EEPROM answer
    // This is a 64x16bit EEPROM, answer is 16 bits long
    for ( i = 15 ; i >= 0 ; --i )
    {
        r8139dn_w8 ( EE_CR, flags | EE_CR_EESK ); // Clock high (each time we'll get one answer bit)
        udelay ( 1 );
        bit = ( r8139dn_r8 ( EE_CR ) & EE_CR_EEDO ) ? 1 : 0; // Fetch the answer bit
        // Right operand isn't of type __le16, so we cast and use __force to avoid Sparse warning
        // This cast is harmless as we're only doing basic arithmetic here, it's endianness independent
        res |= ( __force __le16 ) ( bit << i ); // Append the bit to the result
        r8139dn_w8 ( EE_CR, flags ); // Clock low
        udelay ( 1 );
    }

    // Read is complete
    // Disable EEPROM Programming mode and return to normal mode
    r8139dn_w8 ( EE_CR, EE_CR_NORMAL );
    udelay ( 1 );

    // EEPROM content is stored in Little Endian fashion
    // Let's be endianness independent and convert this to our CPU byte order
    return le16_to_cpu ( res );
}

// Put the MAC from the kernel network device to the hardware registers (IDR)
// The device will then start to consider its own MAC is the one provided
void r8139dn_hw_kernel_mac_to_regs ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );

    // When describing IDR or 93C46CR registers, datasheet fails to mention IDR are write protected
    // However a careful reader will notice the "W*" in front of the IDR registers in the
    // summary table of the EEPROM registers in section 6.1.
    // We must first unlock the config registers before any change can be attempted on IDR
    r8139dn_w8 ( EE_CR, EE_CR_CFG_WRITE_ENABLE );

    // Datasheet says than when writing to IDR0~5, we have to use 4-byte access
    // What is meant is two dword accesses (2 x 32 bit)
    // Trying to write byte per byte results in random corruption in these registers
    //
    // We use a special RAW variant when writing here, because we want to bypass the usual cpu_to_le32
    // We just want to write the memory as it is currently stored in our RAM (regardless of whether we are LE or BE)
    // Doing this is OK because no matter BE/LE, our dev_addr always has the same layout in memory (char *)
    // The Most Significant Byte of the MAC address is always in our dev_addr [ 0 ]
    // We could also have used a le32_to_cpu, but this would have caused a useless double-byteswap for BE CPU
    // Here, we make no swap on LE, and no swap on BE :)
    __r8139dn_w32_raw ( IDR0, ( ( u32 * ) ndev -> dev_addr ) [ 0 ] );

    // We should not worry writing to IDR0 + 6 and IDR0 + 7: they are reserved for this purpose
    __r8139dn_w32_raw ( IDR4, ( ( u32 * ) ndev -> dev_addr ) [ 1 ] );

    // Relock the config registers to prevent accidental changes
    r8139dn_w8 ( EE_CR, EE_CR_NORMAL );
}

// Retrieve the MAC address from device's EEPROM
// and update the net device with it to tell the kernel.
void r8139dn_hw_eeprom_mac_to_kernel ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );
    int i;

    for ( i = 0 ; i < 3 ; ++i )
    {
        ( ( u16 * ) ndev -> dev_addr ) [ i ] = _r8139dn_hw_eeprom_read ( priv, EE_DATA_MAC + i );
    }
}

// Enable the transmitter, set up the transmission settings
// Tell hardware where to DMA
void r8139dn_hw_setup_tx ( struct r8139dn_priv * priv )
{
    int i;
    u8 cr = r8139dn_r8 ( CR );

    // Turn the transmitter on
    r8139dn_w8 ( CR, cr | CR_TE );

    // Set up the TX settings
    r8139dn_w32 ( TCR, priv -> tcr );

    // We want 8 + (3 x 32) bytes = 104 bytes of early TX threshold
    // It means we put data on the wire only once FIFO has reached this threshold
    priv -> tx_flags = ( 3 << TSD_ERTXTH_SHIFT );

    // Inform the hardware about the DMA location of the TX descriptors
    // That way, later it can read the frames we want to send
    for ( i = 0; i < R8139DN_TX_DESC_NB ; ++i )
    {
        r8139dn_w32 ( TSAD0 + i * TSAD_GAP, priv -> tx_ring.dma + i * R8139DN_TX_DESC_SIZE );
    }
}

// Enable the receiver, set up the reception settings
// Tell hardware where to DMA
void r8139dn_hw_setup_rx ( struct r8139dn_priv * priv )
{
    u8 cr = r8139dn_r8 ( CR );

    // Disable Multiple Interrupt (we're going to disable early RX mode in RCR)
    r8139dn_w16 ( MULINT, 0 );

    // Tell the hardware where to DMA (location of the RX buffer)
    // We do this before enabling RX to avoid the NIC starting DMA before it knows the address
    r8139dn_w32 ( RBSTART, priv -> rx_ring.dma );

    // Turn the receiver on
    r8139dn_w8 ( CR, cr | CR_RE );

    // Set up the RX settings
    // We want to receive broadcast frames as well as frames for our own MAC
    r8139dn_w32 ( RCR, RCR_MXDMA_1024 | RCR_APM | RCR_AB | RCR_RBLEN_16K );
}

// Disable transceiver (TX & RX)
// This stops all Master PCI DMA activity
void r8139dn_hw_disable_transceiver ( struct r8139dn_priv * priv )
{
    r8139dn_w8 ( CR, 0 );
}

// Ask the device to enable interrupts
void r8139dn_hw_enable_irq ( struct r8139dn_priv * priv )
{
    r8139dn_w16 ( IMR, priv -> interrupts );
}

// Ask the device to disable interrupts
void r8139dn_hw_disable_irq ( struct r8139dn_priv * priv )
{
    r8139dn_w16 ( IMR, 0 );
}

// Configure the leds
// led_cfg should be one of the CFG1_LEDS_<0>_<1>_<2> where each number
// is to be replaced by the function to assign to that LED
// My PCI adaptor doesn't have a LED2
void r8139dn_hw_configure_leds ( struct r8139dn_priv * priv, u8 led_cfg )
{
    // Fetch CONFIG1 with LEDS bit zeroed
    u8 cfg1 = r8139dn_r8 ( CONFIG1 ) & ~ CFG1_LEDS_MASK;

    // CONFIG1 is write protected. Let's enable write
    r8139dn_w8 ( EE_CR, EE_CR_CFG_WRITE_ENABLE );
    {
        // Configure the leds as requested, but be careful about led_cfg value
        // We want to avoid changing CONFIG1 bits not related to the leds
        r8139dn_w8 ( CONFIG1, cfg1 | ( led_cfg & CFG1_LEDS_MASK ) );
    }
    // Put config registers back to read-only mode
    r8139dn_w8 ( EE_CR, EE_CR_NORMAL );
}

// Convert the chipset version number to an understandable string
const char * r8139dn_hw_version_str ( u32 version )
{
    switch ( version )
    {
        case RTL8139:
            return "RTL8139";
        case RTL8139A:
            return "RTL8139A";
        case RTL8139AG_C:
            return "RTL8139AG/8139C";
        case RTL8139B_8130:
            return "RTL8139B/8130";
        case RTL8100:
            return "RTL8100";
        case RTL8100B_8139D:
            return "RTL8100B/8139D";
        case RTL8139CP:
            return "RTL8139C+";
        case RTL8101:
            return "RTL8101";

        default:
            return "Unknown";
    }
}
