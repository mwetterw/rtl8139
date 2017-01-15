#include "hw.h"

void r8139dn_hw_reset ( struct r8139dn_priv * priv )
{
    // Ask the chip to reset
    r8139dn_w8 ( CR, CR_RST );

    // Wait until the reset is complete
    // The chip notify us by clearing the bit
    while ( r8139dn_r8 ( CR ) & ( CR_RST ) )
    {
        udelay ( 1 );
    }
}
