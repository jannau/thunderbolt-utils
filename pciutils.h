// SPDX-License-Identifier: LGPL-2.0

/*
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include "passthrough.h"

#define VDID_LEN		4
#define TRIM_VDID_PATH		10

#define INTEL_VID		"8086"

#define PCI_CMD			0x4
#define PCI_CMD_MEM		0x2
#define PCI_CMD_MASTER		0x4

extern char *pci_drv_sysfs_path;
extern char *pci_dev_sysfs_path;

struct vdid {
	char vendor_id[MAX_LEN];
	char device_id[MAX_LEN];
};

struct pci_vdid {
	char *pci_id;
	struct vdid *vdid;
};

void do_pci_rescan(void);
void remove_pci_dev(const char *pci_id);
struct vdid* get_vdid(const char *pci_id);
void allow_bus_master(const char *pci_id);
u64 total_grp_modules(const char *pci_id);
