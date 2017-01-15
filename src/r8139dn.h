// BAR, Base Address Registers in the PCI Configuration Space
enum
{
    R8139DN_IOAR,   // BAR0 (IO Ports, PMIO)
    R8139DN_MEMAR   // BAR1 (Memory,   MMIO)
                    // BAR2 -> BAR5 are unused (all 0)
};

// IOAR or MEMAR each need at least 256 bytes
#define R8139DN_IO_SIZE 0x100
