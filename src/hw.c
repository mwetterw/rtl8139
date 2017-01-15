#include "hw.h"

void r8139dn_hw_reset ( struct r8139dn_priv * priv )
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
}
