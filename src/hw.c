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
