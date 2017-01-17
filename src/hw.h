#ifndef _R8139DN_HW_H
#define _R8139DN_HW_H

#include <linux/netdevice.h>
#include <linux/io.h>
#include "net.h"

void r8139dn_hw_reset ( struct r8139dn_priv * priv );
void r8139dn_hw_mac_load_to_kernel ( struct net_device * ndev );
void r8139dn_hw_enable_and_configure_tx ( struct r8139dn_priv * priv );
void r8139dn_hw_disable_transceiver ( struct r8139dn_priv * priv );

// BAR, Base Address Registers in the PCI Configuration Space
enum
{
    R8139DN_IOAR,   // BAR0 (IO Ports, PMIO)
    R8139DN_MEMAR   // BAR1 (Memory,   MMIO)
                    // BAR2 -> BAR5 are unused (all 0)
};

// IOAR or MEMAR each need at least 256 bytes
#define R8139DN_IO_SIZE 256

// Maximum Ethernet frame size that can be handled by the device
#define R8139DN_MAX_ETH_SIZE 1792

// Number of TX descriptors
#define R8139DN_TX_DESC_NB 4

#define R8139DN_TX_DMA_SIZE ( R8139DN_MAX_ETH_SIZE * R8139DN_TX_DESC_NB )

enum
{
    IDR0      = 0x00, // ID Registers.
    IDR1      = 0x01, // W access only 32. R access 8, 16 or 32.
    IDR2      = 0x02, // They contain the MAC address currently
    IDR3      = 0x03, // configured. The initial value is auto-loaded
    IDR4      = 0x04, // from EEPROM.
    IDR5      = 0x05,

    // Reserved 0x06,
    // Reserved 0x07,

    MAR0      = 0x08, // Multicast Registers
    MAR1      = 0x09, // W access only 32. R access 8, 16 or 32.
    MAR2      = 0x0a,
    MAR3      = 0x0b,
    MAR4      = 0x0c,
    MAR5      = 0x0d,
    MAR6      = 0x0e,
    MAR7      = 0x0f,

    // Transmission Status Registers
    TSD0      = 0x10,
    TSD1      = 0x14,
    TSD2      = 0x18,
    TSD3      = 0x1c,

    TSAD0     = 0x20,
    TSAD1     = 0x24,
    TSAD2     = 0x28,
    TSAD3     = 0x2c,

    RBSTART   = 0x30,
    ERBCR     = 0x34,
    ERSR      = 0x36,

    // Command Register
    CR        = 0x37,
        // Reserved 7 -> 5
        CR_RST   = ( 1 << 4 ),
        CR_RE    = ( 1 << 3 ),
        CR_TE    = ( 1 << 2 ),
        // Reserved       1
        CR_BUFE  = ( 1 << 0 ),

    CAPR      = 0x38,
    CBR       = 0x3a,

    IMR       = 0x3c,
    ISR       = 0x3e,

    // TX Configuration Register
    TCR       = 0x40,
        // Interframe Gap Time
        TCR_IFG_SHIFT       = 24,
            TCR_IFG_84      = ( 0 << TCR_IFG_SHIFT ), // 8.4 us / 840 ns
            TCR_IFG_88      = ( 1 << TCR_IFG_SHIFT ), // 8.8 us / 880 ns
            TCR_IFG_92      = ( 2 << TCR_IFG_SHIFT ), // 9.2 us / 920 ns
            TCR_IFG_96      = ( 3 << TCR_IFG_SHIFT ), // 9.6 us / 960 ns
            TCR_IFG_DEFAULT = TCR_IFG_96,
        // Append FCS at the end of the frame?
        TCR_CRC             = ( 1 << 16 ),
        // Max DMA Burst (16 -> 2048 bytes)
        TCR_MXDMA_SHIFT     = 8,
            TCR_MXDMA_16    = ( 0 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_32    = ( 1 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_64    = ( 2 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_128   = ( 3 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_256   = ( 4 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_512   = ( 5 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_1024  = ( 6 << TCR_MXDMA_SHIFT ),
            TCR_MXDMA_2048  = ( 7 << TCR_MXDMA_SHIFT ),

    RCR       = 0x44,

    TCTR      = 0x48,

    MPC       = 0x4c,

    EEPROM_CR = 0x50,

    CONFIG0   = 0x51,
    CONFIG1   = 0x52,
    // Reserved 0x53,
    TIMERINT  = 0x54,

    MSR       = 0x58,

    CONFIG3   = 0x59,
    CONFIG4   = 0x5a,

    // Reserved 0x5b,

    MULINT    = 0x5c,
    RERID     = 0x5e,

    // Reserved 0x5f,

    TSAD      = 0x60,

    // PHY registers START
    BMCR      = 0x62,
    BMSR      = 0x64,

    ANAR      = 0x66,
    ANLPAR    = 0x68,
    ANER      = 0x6a,

    DIS       = 0x6c,

    FCSC      = 0x6e,

    NWAYTR    = 0x70,

    RXERCNT   = 0x72,
    CSCR      = 0x74,
    // PHY registers END

    // Reserved 0x76,
    // Reserved 0x77,

    PHY1_PARM = 0x78,
    TW_PARM   = 0x7c,
    PHY2_PARM = 0x80,

    // Reserved 0x81,
    // Reserved 0x82,
    // Reserved 0x83,

    CRC0      = 0x84,
    CRC1      = 0x85,
    CRC2      = 0x86,
    CRC3      = 0x87,
    CRC4      = 0x88,
    CRC5      = 0x89,
    CRC6      = 0x8a,
    CRC7      = 0x8b,

    WAKEUP0   = 0x8c,
    WAKEUP1   = 0x94,
    WAKEUP2   = 0x9c,
    WAKEUP3   = 0xa4,
    WAKEUP4   = 0xac,
    WAKEUP5   = 0xb4,
    WAKEUP6   = 0xbc,
    WAKEUP7   = 0xc4,

    LSBCRC0   = 0xcc,
    LSBCRC1   = 0xcd,
    LSBCRC2   = 0xce,
    LSBCRC3   = 0xcf,
    LSBCRC4   = 0xd0,
    LSBCRC5   = 0xd1,
    LSBCRC6   = 0xd2,
    LSBCRC7   = 0xd3,

    // Reserved 0xd4,
    // Reserved 0xd5,
    // Reserved 0xd6,
    // Reserved 0xd7,

    CONFIG5   = 0xd8,

    // Reserved until 0xff
};

#define r8139dn_r8(reg)  ioread8  ( priv->mmio + ( reg ) )
#define r8139dn_r16(reg) ioread16 ( priv->mmio + ( reg ) )
#define r8139dn_r32(reg) ioread32 ( priv->mmio + ( reg ) )

#define r8139dn_w8(reg,val)  iowrite8  ( ( val ), priv->mmio + ( reg ) )
#define r8139dn_w16(reg,val) iowrite16 ( ( val ), priv->mmio + ( reg ) )
#define r8139dn_w32(reg,val) iowrite32 ( ( val ), priv->mmio + ( reg ) )

#endif
