#ifndef _R8139DN_PCI_H
#define _R8139DN_PCI_H

#include <linux/pci.h>

int r8139dn_pci_probe ( struct pci_dev * pdev, const struct pci_device_id * id );
void r8139dn_pci_remove ( struct pci_dev * pdev );

extern struct pci_driver r8139dn_pci_driver;

#endif
