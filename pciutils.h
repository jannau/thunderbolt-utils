#include <stdbool.h>

#include "pciutils/lib/pci.h"

struct pci_access* init_pci(void);
void clean_pci(struct pci_access *pacc);
bool find_host_controller(void);
