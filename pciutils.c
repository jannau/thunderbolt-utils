#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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

/* Make the respective PCIe device use DMA */
void allow_bus_master(const char *pci_id)
{
	char path[MAX_LEN];
	char *root_cmd;

	/* To avoid any conflicts, set the MM mapping accessibility also */
	snprintf(path, sizeof(path), "setpci -s %s 0x%x.B=0x%x", pci_id, PCI_CMD,
		 PCI_CMD_MASTER | PCI_CMD_MEM);

	root_cmd = switch_cmd_to_root(path);
	do_bash_cmd(root_cmd);
}
