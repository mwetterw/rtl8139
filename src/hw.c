#include "hw.h"

void r8139dn_hw_reset ( struct r8139dn_priv * priv )
{
    r8139dn_w8 ( CR, 0x10 );
    while ( r8139dn_r8 ( CR ) & ( 0x10 ) )
    {
        udelay ( 1 );
    }
}
