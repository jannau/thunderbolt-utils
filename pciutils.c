#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "pciutils.h"

static u32 host_class_id = 0x0c0340;

struct pci_access* init_pci(void)
{
	struct pci_access *pacc;

	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);

	return pacc;
}

void clean_pci(struct pci_access *pacc)
{
	pci_cleanup(pacc);
}

bool find_host_controller(void)
{
	struct pci_access *pacc;
	struct pci_dev *pdev;
	bool present = false;
	u32 class_id;

	pacc = init_pci();

	for (pdev = pacc->devices; pdev; pdev = pdev->next) {
		class_id = pci_read_long(pdev, PCI_CLASS_REVISION);
		class_id = class_id >> 8;
		if (class_id == host_class_id) {
			printf("found USB4 host controller: %04x:%02x:%02x.%d\n", pdev->domain,
				pdev->bus, pdev->dev, pdev->func);

			present = true;
		}
	}

	clean_pci(pacc);

	return present;
}
