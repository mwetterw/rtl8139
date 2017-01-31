#ifndef _R8139DN_HW_H
#define _R8139DN_HW_H

#include <linux/netdevice.h>
#include <linux/io.h>
#include "net.h"

void r8139dn_hw_reset ( struct r8139dn_priv * priv );
void r8139dn_hw_eeprom_mac_to_kernel ( struct net_device * ndev );
void r8139dn_hw_kernel_mac_to_regs ( struct net_device * ndev );
void r8139dn_hw_setup_tx ( struct r8139dn_priv * priv );
void r8139dn_hw_disable_transceiver ( struct r8139dn_priv * priv );
void r8139dn_hw_enable_irq ( struct r8139dn_priv * priv );
void r8139dn_hw_clear_irq ( struct r8139dn_priv * priv );
void r8139dn_hw_disable_irq ( struct r8139dn_priv * priv );
u16 r8139dn_eeprom_read ( struct r8139dn_priv * priv, u8 word_addr );
const char * r8139dn_hw_version_str ( u32 version );

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

// Number and size of TX descriptors
#define R8139DN_TX_DESC_NB 4
#define R8139DN_TX_DESC_SIZE R8139DN_MAX_ETH_SIZE
#define R8139DN_TX_DMA_SIZE ( R8139DN_TX_DESC_SIZE * R8139DN_TX_DESC_NB )

// Registers
enum
{
    // ID Registers ("What is my own MAC register")
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
    TSD_GAP  = ( TSD1 - TSD0 ),
        TSD_CRS    = ( 1 << 31 ),
        TSD_TABT   = ( 1 << 30 ),
        TSD_OWC    = ( 1 << 29 ),
        TSD_CDH    = ( 1 << 28 ),
        TSD_NCC    = ( 0xff << 24 ),
        // Reserved 23 -> 22
        TSD_ERTXTH_SHIFT = 16,
            TSD_ERTXTH = ( 0x3f << TSD_ERTXTH_SHIFT ),
        TSD_TOK    = ( 1 << 15 ),
        TSD_TUN    = ( 1 << 14 ),
        TSD_OWN    = ( 1 << 13 ),
        TSD_SIZE   = ( 0x1fff ),

    // Transmit Start Address of Descriptor Registers
    TSAD0     = 0x20,
    TSAD1     = 0x24,
    TSAD2     = 0x28,
    TSAD3     = 0x2c,
    TSAD_GAP  = ( TSAD1 - TSAD0 ),

    RBSTART   = 0x30,
    ERBCR     = 0x34,
    ERSR      = 0x36,

    // Command Register
    CR        = 0x37,
        // Reserved  7 -> 5
        CR_RST   = ( 1 << 4 ),
        CR_RE    = ( 1 << 3 ),
        CR_TE    = ( 1 << 2 ),
        // Reserved       1
        CR_BUFE  = ( 1 << 0 ),

    CAPR      = 0x38,
    CBR       = 0x3a,

    // Interrupt Mask Register
    IMR       = 0x3c,
        // Valid for IMR and ISR
        INT_SERR    = ( 1 << 15 ),
        INT_TIMEOUT = ( 1 << 14 ),
        INT_LENCHG  = ( 1 << 13 ),
        // Reserved     12 -> 7
        INT_FOVW    = ( 1 << 6 ),
        INT_PUN     = ( 1 << 5 ),
        INT_RXOVW   = ( 1 << 4 ),
        INT_TER     = ( 1 << 3 ),
        INT_TOK     = ( 1 << 2 ),
        INT_RER     = ( 1 << 1 ),
        INT_ROK     = ( 1 << 0 ),
        INT_CLEAR   = 0xffff,
    // Interrupt Status Register
    ISR       = 0x3e,

    // TX Configuration Register
    TCR       = 0x40,
        TCR_HWVERID_MASK    = 0x7cc00000,
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

    // EEPROM (93C46) Control Register
    EE_CR = 0x50,
        EE_CR_NORMAL    = 0x00, // Normal mode (network/host communication)
        EE_CR_AUTO_LOAD = 0x40, // 93C46 contents auto-loaded (take 2ms), then goes back to normal mode.
        EE_CR_PROGRAM   = 0x80, // EECS, EESK, EEDI, EEDO will reflect the real EEPROM pins
            EE_CR_EECS = 0x08, // Chip Select
            EE_CR_EESK = 0x04, // Serial Data Clock
            EE_CR_EEDI = 0x02, // Serial Data Input
            EE_CR_EEDO = 0x01, // Serial Data Output

            // Following come from ATMEL AT93C46 EEPROM Datasheet
            // Start Bit | Opcode | Address [| Data]
            // The SB (Start Bit) is included in the following opcodes
            EE_CMD_READ  = 0x06, // Reads data stored in memory
            EE_CMD_EWEN  = 0x04, // Write enable (must precede all programming modes)
                EE_CMD_EWEN_ADDR  = 0x30,
            EE_CMD_ERASE = 0x07, // Erases memory location
            EE_CMD_WRITE = 0x05, // Writes memory location. After address: 16bits of data to write
            EE_CMD_ERAL  = 0x04, // Erases all memory locations
                EE_CMD_ERAL_ADDR  = 0x20,
            EE_CMD_WRAL  = 0x04, // Writes all memory locations. After address: 16bits of data to write
                EE_CMD_WRAL_ADDR  = 0x10,
            EE_CMD_EWDS  = 0x04, // Disables all programming instructions
                EE_CMD_EWDS_ADDR = 0x00,
            EE_ADDRLEN   = 0x06, // Size of address after the start bit and opcode
            EE_OPCODELEN = 0x03, // Size of opcode (with Start Bit included)
            EE_CMD_READ_LEN  = ( EE_OPCODELEN + EE_ADDRLEN ),

            // Address of data inside the EEPROM (in word)
            EE_DATA_MAC = 0x07,

        EE_CR_CFG_WRITE_ENABLE = 0xc0, // Unlock write access to IDR0~5, CONFIG0~4 and bit 13,12,8 of BCMR

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

    // Transmit Status of All Descriptors
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

// Helpers to decode HWVERID in TCR (chipset versions)
enum
{
    RTL8139        = BIT ( 30 ) | BIT ( 29 ),
    RTL8139A       = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ),
    RTL8139AG_C    = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ) | BIT ( 26 ),
    RTL8139B_8130  = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ) | BIT ( 27 ),
    RTL8100        = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ) | BIT ( 27 ) | BIT ( 23 ),
    RTL8100B_8139D = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ) | BIT ( 26 ) | BIT ( 22 ),
    RTL8139CP      = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ) | BIT ( 26 ) | BIT ( 23 ),
    RTL8101        = BIT ( 30 ) | BIT ( 29 ) | BIT ( 28 ) | BIT ( 26 ) | BIT ( 23 ) | BIT ( 22 ),
};

#define r8139dn_r8(reg)  ioread8  ( priv->mmio + ( reg ) )
#define r8139dn_r16(reg) ioread16 ( priv->mmio + ( reg ) )
#define r8139dn_r32(reg) ioread32 ( priv->mmio + ( reg ) )

#define r8139dn_w8(reg,val)  iowrite8  ( ( val ), priv->mmio + ( reg ) )
#define r8139dn_w16(reg,val) iowrite16 ( ( val ), priv->mmio + ( reg ) )
#define r8139dn_w32(reg,val) iowrite32 ( ( val ), priv->mmio + ( reg ) )

// Write without swapping byte order from CPU to Little Endian
// No matter whether we are a BE or LE CPU, write data as is stored in our RAM to the device registers
#define __r8139dn_w16_raw(reg,val) __raw_writew ( ( val ), priv->mmio + ( reg ) )
#define __r8139dn_w32_raw(reg,val) __raw_writel ( ( val ), priv->mmio + ( reg ) )

#endif
