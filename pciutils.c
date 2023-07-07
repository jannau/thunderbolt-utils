// SPDX-License-Identifier: LGPL-2.0
/*
 * PCI utilities
 *
 * This file provides functionalities to perform PCI-related operations
 * on the thunderbolt host IP including bus mastering, addition/removal, etc.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pciutils.h"

char *pci_drv_sysfs_path = "/sys/bus/pci/drivers/";
char *pci_dev_sysfs_path = "/sys/bus/pci/devices/";

/* Perform a PCI rescan */
void do_pci_rescan(void)
{
	char *path = "echo 1 > /sys/bus/pci/rescan";
	char *root_cmd, *bash_op;

	if (is_link_nabs("/sys/bus/pci/rescan"))
		exit(1);

	root_cmd = switch_cmd_to_root(path);
	bash_op = do_bash_cmd(root_cmd);

	free(root_cmd);
	free(bash_op);
}

/* Remove the provided PCI device from the system */
void remove_pci_dev(const char *pci_id)
{
	char path[MAX_LEN], check[MAX_LEN];
	char *root_cmd, *bash_op;

	snprintf(check, sizeof(check), "%s%s/remove", pci_dev_sysfs_path, pci_id);
	if (is_link_nabs(check))
		exit(1);

	snprintf(path, sizeof(path), "echo 1 > %s%s/remove", pci_dev_sysfs_path,
		 pci_id);
	root_cmd = switch_cmd_to_root(path);
	bash_op = do_bash_cmd(root_cmd);

	free(root_cmd);
	free(bash_op);
}

/* Get vendor and device IDs for the given PCI device */
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

	free(vdid_char);
	free(bash_result);

	return vdid;
}

/* Make the respective PCIe device use DMA */
void allow_bus_master(const char *pci_id)
{
	char *root_cmd, *bash_op;
	char path[MAX_LEN];

	/* To avoid any conflicts, set the MM mapping accessibility also */
	snprintf(path, sizeof(path), "setpci -s %s 0x%x.B=0x%x", pci_id, PCI_CMD,
		 PCI_CMD_MASTER | PCI_CMD_MEM);

	root_cmd = switch_cmd_to_root(path);
	bash_op = do_bash_cmd(root_cmd);

	free(root_cmd);
	free(bash_op);
}

/* Returns the total modules present in the same IOMMU group as the given PCI device */
u64 total_grp_modules(const char *pci_id)
{
	struct list_item *list_modules;
	char path[MAX_LEN];
	u64 ret;

	snprintf(path, sizeof(path), "for line in $(ls %s%s/iommu_group/devices); do echo $line; done",
		 pci_dev_sysfs_path, pci_id);

	list_modules = do_bash_cmd_list(path);
	ret = get_total_list_items(list_modules);

	free_list(list_modules);
	return ret;
}
