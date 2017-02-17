#ifndef _R8139DN_HW_H
#define _R8139DN_HW_H

#include <linux/netdevice.h>
#include <linux/io.h>

#include "hw_regs.h"

struct r8139dn_priv;

int r8139dn_hw_reset ( struct r8139dn_priv * priv );
void r8139dn_hw_eeprom_mac_to_kernel ( struct net_device * ndev );
void r8139dn_hw_kernel_mac_to_regs ( struct net_device * ndev );
void r8139dn_hw_setup_tx ( struct r8139dn_priv * priv );
void r8139dn_hw_setup_rx ( struct r8139dn_priv * priv );
void r8139dn_hw_disable_transceiver ( struct r8139dn_priv * priv );
void r8139dn_hw_enable_irq ( struct r8139dn_priv * priv );
void r8139dn_hw_ack_irq ( struct r8139dn_priv * priv );
void r8139dn_hw_disable_irq ( struct r8139dn_priv * priv );
void r8139dn_hw_configure_leds ( struct r8139dn_priv * priv, u8 led_cfg );
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
#define R8139DN_MAX_MTU ( R8139DN_MAX_ETH_SIZE - ETH_HLEN - ETH_FCS_LEN )

// Number and size of TX descriptors
#define R8139DN_TX_DESC_NB 4 // Warning: we use a property requiring this to be a power of 2
#define R8139DN_TX_DESC_SIZE R8139DN_MAX_ETH_SIZE
#define R8139DN_TX_DMA_SIZE ( R8139DN_TX_DESC_SIZE * R8139DN_TX_DESC_NB )

// RX DMA size
#define R8139DN_RX_PAD 16
#define R8139DN_RX_BUFLEN 16384
#define R8139DN_RX_DMA_SIZE ( R8139DN_RX_BUFLEN + R8139DN_RX_PAD )
#define R8139DN_RX_HEADER_SIZE 4
#define R8139DN_RX_ALIGN_ADD 3
#define R8139DN_RX_ALIGN_MASK ( ~R8139DN_RX_ALIGN_ADD )
#define R8139DN_RX_ALIGN(val) ( ( ( val ) + R8139DN_RX_ALIGN_ADD ) & R8139DN_RX_ALIGN_MASK )

struct r8139dn_rx_header
{
    u16 status;
    u16 size;
};


// Macros to read / write the network card registers
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
