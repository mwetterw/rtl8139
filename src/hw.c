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

// Retrieve the MAC address currently in the IDR registers
// and update the net device with it to tell the kernel.
// TODO Read from EEPROM instead
void r8139dn_hw_mac_load_to_kernel ( struct net_device * ndev )
{
    struct r8139dn_priv * priv = netdev_priv ( ndev );

    // XXX Endianness?
    ( ( u32 * ) ndev -> dev_addr ) [ 0 ] = r8139dn_r32 ( IDR0 );
    ( ( u16 * ) ndev -> dev_addr ) [ 2 ] = r8139dn_r16 ( IDR4 );
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
