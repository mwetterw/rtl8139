#include "hw.h"

// Ask the hardware to reset
// This will disable TX and RX, reset FIFOs,
// reset TX buffer at TSAD0, and set BUFE (RX buffer is empty)
// Note: IDR0 -> 5 and MAR0 -> 5 are not reset
void r8139dn_hw_reset ( struct r8139dn_priv * priv )
{
    int i = 1000;

    // Ask the chip to reset
    r8139dn_w8 ( CR, CR_RST );

    // Wait until the reset is complete or timeout
    // The chip notify us by clearing the bit
    while ( --i )
    {
        // XXX Memory or Compiler Barrier needed?
        if ( ! ( r8139dn_r8 ( CR ) & CR_RST ) )
        {
            // Reset is complete!
            break;
        }
        udelay ( 1 );
    }

    // Resetting the chip also resets hardware TX pointer to TSAD0
    // So we need to keep track of this, and we also reset our own position
    priv -> tx_buffer_hw_pos = 0;
    priv -> tx_buffer_our_pos = 0;
}

// Read a word (16 bits) from the EEPROM at word_addr address
// We could actually also use <linux/eeprom_93cx6.h> :)
// EEPROM content is in the Little Endian fashion
// But I make sure I return to you the value in your native CPU byte-order
u16 r8139dn_eeprom_read ( struct r8139dn_priv * priv, u8 word_addr )
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
    // We should not worry writing to IDR0 + 6 and IDR0 + 7: they are reserved for this purpose
    r8139dn_w32 ( IDR0, ( ( u32 * ) ndev -> dev_addr ) [ 0 ] );
    r8139dn_w32 ( IDR4, ( ( u32 * ) ndev -> dev_addr ) [ 1 ] );

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
        ( ( u16 * ) ndev -> dev_addr ) [ i ] = r8139dn_eeprom_read ( priv, EE_DATA_MAC + i );
    }
}

// Enable the transmitter, set up the transmission settings
// and tell hardware where to DMA
void r8139dn_hw_setup_tx ( struct r8139dn_priv * priv )
{
    int i;

    // Turn the transmitter on
    r8139dn_w8 ( CR, CR_TE );

    // Set up the TX settings
    r8139dn_w32 ( TCR, TCR_IFG_DEFAULT | TCR_MXDMA_1024 );

    // We want 8 + (3 x 32) bytes = 104 bytes of early TX threshold
    // It means we put data on the wire only once FIFO has reached this threshold
    priv -> tx_flags = ( 3 << TSD_ERTXTH_SHIFT );

    // Inform the hardware about the DMA location of the TX descriptors
    // That way, later it can read the frames we want to send
    for ( i = 0; i < R8139DN_TX_DESC_NB ; ++i )
    {
        r8139dn_w32 ( TSAD0 + i * TSAD_GAP,
                ( priv -> tx_buffer_dma ) + ( i * R8139DN_TX_DESC_SIZE ) );
    }
}

// Disable transceiver (TX & RX)
void r8139dn_hw_disable_transceiver ( struct r8139dn_priv * priv )
{
    r8139dn_w8 ( CR, 0 );
}

// Ask the device to enable interrupts
void r8139dn_hw_enable_irq ( struct r8139dn_priv * priv )
{
    r8139dn_w16 ( IMR, INT_TER | INT_TOK );
}

// Clear interrupts
void r8139dn_hw_clear_irq ( struct r8139dn_priv * priv )
{
    r8139dn_w16 ( ISR, INT_CLEAR );
}

// Ask the device to disable interrupts
void r8139dn_hw_disable_irq ( struct r8139dn_priv * priv )
{
    r8139dn_w16 ( IMR, 0 );
}
