//#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pciutils.h"

void do_pci_rescan()
{
	char *path = "echo 1 > /sys/bus/pci/rescan";

	do_bash_cmd(switch_cmd_to_root(path));
}

void remove_pci_dev(const char *pci_id)
{
	char path[MAX_LEN];

	snprintf(path, sizeof(path), "echo 1 > %s%s/remove", pci_dev_sysfs_path,
		 pci_id);
	do_bash_cmd(switch_cmd_to_root(path));
}

struct vdid* get_vdid(const char *pci_id)
{
	char *vdid_char = malloc(MAX_LEN * sizeof(char));
	struct vdid *vdid = malloc(sizeof(struct vdid));
	char path[MAX_LEN];
	char *bash_result;
	u16 pos;

	snprintf(path, sizeof(path), "lspci -n -s %s", pci_id);

	bash_result = do_bash_cmd(path);

	pos = strpos(bash_result, INTEL_VID, 0);

	strncpy(vdid_char, bash_result + pos, TRIM_VDID_PATH - 1);

	strncpy(vdid->vendor_id, vdid_char, VDID_LEN);
	strncpy(vdid->device_id, vdid_char + VDID_LEN + 1, VDID_LEN);
	vdid->vendor_id[VDID_LEN] = '\0';
	vdid->device_id[VDID_LEN] = '\0';
	return vdid;
}




/*static u32 host_class_id = 0x0c0340;

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
}*/
