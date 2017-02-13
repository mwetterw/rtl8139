// Registers
enum r8139dn_hw_regs
{
    // ID Registers (Unicast filter)
    IDR0      = 0x00,
    IDR1      = 0x01,
    IDR2      = 0x02,
    IDR3      = 0x03,
    IDR4      = 0x04,
    IDR5      = 0x05,

    // Multicast Registers
    MAR0      = 0x08,
    MAR1      = 0x09,
    MAR2      = 0x0a,
    MAR3      = 0x0b,
    MAR4      = 0x0c,
    MAR5      = 0x0d,
    MAR6      = 0x0e,
    MAR7      = 0x0f,

    // Transmission Status of Descriptor Registers
    TSD0      = 0x10,
    TSD1      = 0x14,
    TSD2      = 0x18,
    TSD3      = 0x1c,

    // Transmit Start Address of Descriptor Registers
    TSAD0     = 0x20,
    TSAD1     = 0x24,
    TSAD2     = 0x28,
    TSAD3     = 0x2c,

    RBSTART   = 0x30,
    ERBCR     = 0x34,
    ERSR      = 0x36,
    CR        = 0x37, // Command Register
    CAPR      = 0x38,
    CBR       = 0x3a,
    IMR       = 0x3c, // Interrupt Mask Register
    ISR       = 0x3e, // Interrupt Status Register
    TCR       = 0x40, // TX Configuration Register
    RCR       = 0x44,
    TCTR      = 0x48,
    MPC       = 0x4c,
    EE_CR     = 0x50, // EEPROM (93C46) Control Register
    CONFIG0   = 0x51,
    CONFIG1   = 0x52,
    // Reserved 0x53,
    TIMERINT  = 0x54,
    MSR       = 0x58, // Media Status Register
    CONFIG3   = 0x59,
    CONFIG4   = 0x5a,
    // Reserved 0x5b,
    MULINT    = 0x5c,
    RERID     = 0x5e,
    // Reserved 0x5f,
    TSAD      = 0x60, // Transmit Status of All Descriptors

    // PHY registers
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

// Transmit Status of Descriptor x Register
enum TSDx
{
    TSD_GAP  = ( TSD1 - TSD0 ),
        TSD_CRS    = ( 1 << 31 ),       // Carrier Sense Lost during TX
        TSD_TABT   = ( 1 << 30 ),       // TX Abort, because TX Retry Count has been reached
        TSD_OWC    = ( 1 << 29 ),       // Out of Window Collision during TX
        TSD_CDH    = ( 1 << 28 ),       // NIC failed to send CD Heart Beat
        TSD_NCC    = ( 0xff << 24 ),    // Number of Collision Count
        // Reserved 23 -> 22
        TSD_ERTXTH_SHIFT = 16,          // Early TX Threshold (TX FIFO Threshold)
            TSD_ERTXTH = ( 0x3f << TSD_ERTXTH_SHIFT ),
        TSD_TOK    = ( 1 << 15 ),       // TX OK (TX happened successfully)
        TSD_TUN    = ( 1 << 14 ),       // TX Underrun (ISR can be TOK or TER)
        TSD_OWN    = ( 1 << 13 ),       // Own (DMA transfer completed)
        TSD_SIZE   = ( 0x1fff ),        // Size of Ethernet frame to send
};

// Transmit Start Address of Descriptor x Register
enum TSADx
{
    TSAD_GAP  = ( TSAD1 - TSAD0 ),
};

// Command Register
enum CR
{
    // Reserved  7 -> 5
    CR_RST   = ( 1 << 4 ),
    CR_RE    = ( 1 << 3 ),
    CR_TE    = ( 1 << 2 ),
    // Reserved       1
    CR_BUFE  = ( 1 << 0 ),
};

// Interrupt (Mask or Status) Register
enum IMR_ISR
{
    INT_SERR        = ( 1 << 15 ), // System Error (on the PCI Bus)
    INT_TIMEOUT     = ( 1 << 14 ),
    INT_LENCHG      = ( 1 << 13 ), // Cable Length Changed after RX has been enabled
    // Reserved        12 -> 7
    INT_FOVW        = ( 1 << 6 ), // RX FIFO Overflow
    INT_LNKCHG_PUN  = ( 1 << 5 ), // Link Change / RX Packet Underrun
    INT_RXOVW       = ( 1 << 4 ), // RX Buffer Overflow
    INT_TER         = ( 1 << 3 ), // TX Error (TX Underrun or TX Abort)
    INT_TOK         = ( 1 << 2 ), // TX OK
    INT_RER         = ( 1 << 1 ), // RX Error (CRC or frame alignment error)
    INT_ROK         = ( 1 << 0 ), // RX OK
    INT_CLEAR       = 0xffff,
};

// TX Configuration Register
enum TCR
{
    TCR_HWVERID_MASK    = 0x7cc00000,
    // Interframe Gap Time
    TCR_IFG_SHIFT       = 24,
        TCR_IFG_84      = ( 0 << TCR_IFG_SHIFT ), // 8.4 us / 840 ns
        TCR_IFG_88      = ( 1 << TCR_IFG_SHIFT ), // 8.8 us / 880 ns
        TCR_IFG_92      = ( 2 << TCR_IFG_SHIFT ), // 9.2 us / 920 ns
        TCR_IFG_96      = ( 3 << TCR_IFG_SHIFT ), // 9.6 us / 960 ns
        TCR_IFG_DEFAULT = TCR_IFG_96,
    // Loopback mode
    TCR_LBK_SHIFT       = 17,
        TCR_LBK_DISABLE = ( 0 << TCR_LBK_SHIFT ),
        TCR_LBK_ENABLE  = ( 3 << TCR_LBK_SHIFT ),   // Packets won't really be TXed
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
};

// RX Configuration Register
enum RCR
{
    // Early RX Threshold multiplier of the whole packet (0/16 -> 15/16)
    RCR_ERTH_SHIFT      = 24,
        RCR_ERTH        = ( 0xf << RCR_ERTH_SHIFT ),
    // Reserved        23 -> 18
    RCR_MUL_ER_INT      = ( 1 << 17 ),              // Multiple Early Interrupt
    RCR_RER8            = ( 1 << 16 ),              // Receive Error packet > 8 bytes
    // RX FIFO Threshold
    // Threshold = 2^(4 + RXFTH) bytes. 7: transfer when whole packet is in FIFO
    RCR_RXFTH_SHIFT     = 13,
        RCR_RXFTH       = ( 7 << RCR_RXFTH_SHIFT ),
    // RX Buffer Length
    // Size = 2^(3 + RXLEN) * 1024 + 16 bytes
    RCR_RBLEN_SHIFT     = 11,
        RCR_RBLEN_8208  = ( 0 << RCR_RBLEN_SHIFT ),
        RCR_RBLEN_16400 = ( 1 << RCR_RBLEN_SHIFT ),
        RCR_RBLEN_32784 = ( 2 << RCR_RBLEN_SHIFT ),
        RCR_RBLEN_65552 = ( 3 << RCR_RBLEN_SHIFT ),
    // Max RX DMA Burst Size
    // Burst size = 2^(4 + MXDMA) bytes. 7: unlimited burst size
    RCR_MXDMA_SHIFT     = 8,
        RCR_MXDMA_16    = ( 0 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_32    = ( 1 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_64    = ( 2 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_128   = ( 3 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_256   = ( 4 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_512   = ( 5 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_1024  = ( 6 << RCR_MXDMA_SHIFT ),
        RCR_MXDMA_NOLIM = ( 7 << RCR_MXDMA_SHIFT ),
    RCR_WRAP        = ( 1 << 7 ),
    // Reserved              6
    RCR_AER         = ( 1 << 5 ), // Accept ERror packets (CRC, align, collided)
    RCR_AR          = ( 1 << 4 ), // Accept Runt (packets < 64 bytes but >= 8 bytes)
    RCR_AB          = ( 1 << 3 ), // Accept Broadcast packets
    RCR_AM          = ( 1 << 2 ), // Accept Multicast packets
    RCR_APM         = ( 1 << 1 ), // Accept Physical Match packets
    RCR_AAP         = ( 1 << 0 ), // Accept All packets
};

// Helpers to decode HWVERID in TCR (chipset versions)
enum TCR_HWVERID
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

// EEPROM Control Register
enum EE
{
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
};

// Media Status Register
enum MSR
{
    MSR_TXFCE      = ( 1 << 7 ), // TX Flow Control Enable
    MSR_RXFCE      = ( 1 << 6 ), // TX Flow Control Enable
    // Reserved             5
    MSR_AUX_STATUS = ( 1 << 4 ), // Auxiliary Power Present Status
    MSR_SPD_10     = ( 1 << 3 ), // 0: 100Mps, 1: 10Mps
    MSR_LINK_BAD   = ( 1 << 2 ), // 0: Link OK, 1: Link fail
    MSR_TXPF       = ( 1 << 1 ), // 0: Sends timer done packet, 1: Sends Pause Packet
    MSR_RXPF       = ( 1 << 0 ), // 0: Pause state clear, 1: Backoff state because Pause Packet received
};
