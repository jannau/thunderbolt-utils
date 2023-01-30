#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "passthrough.h"

#define VDID_LEN	4
#define TRIM_VDID_PATH	10

#define INTEL_VID	"8086"

static char *pci_drv_sysfs_path = "/sys/bus/pci/drivers/";
static char *pci_dev_sysfs_path = "/sys/bus/pci/devices/";

static void do_pci_rescan()
{
	char *path = "echo 1 > /sys/bus/pci/rescan";

	do_bash_cmd(switch_cmd_to_root(path));
}

static void bind_vfio_module(const char *pci_id, const struct vdid *vdid)
{
	char dev_path[MAX_LEN];
	char drv_path[MAX_LEN];

	snprintf(dev_path, sizeof(dev_path), "echo %s > %s%s/driver/unbind",
		 pci_id, pci_dev_sysfs_path, pci_id);
	do_bash_cmd(switch_cmd_to_root(dev_path));

	snprintf(drv_path, sizeof(drv_path), "echo '%s %s' > %svfio-pci/new_id",
		 vdid->vendor_id, vdid->device_id, pci_drv_sysfs_path);
	do_bash_cmd(switch_cmd_to_root(drv_path));
}

static void remove_pci_dev(const char *pci_id)
{
	char path[MAX_LEN];

	snprintf(path, sizeof(path), "echo 1 > %s%s/remove", pci_dev_sysfs_path,
		 pci_id);
	do_bash_cmd(switch_cmd_to_root(path));
}

static void unbind_vfio_module(const char *pci_id, const struct vdid *vdid)
{
	char dev_path[MAX_LEN];
	char drv_path[MAX_LEN];

	snprintf(dev_path, sizeof(dev_path), "echo %s > %s%s/driver/unbind",
		 pci_id, pci_dev_sysfs_path, pci_id);
	do_bash_cmd(switch_cmd_to_root(dev_path));

	snprintf(drv_path, sizeof(drv_path), "echo '%s %s' > %svfio-pci/remove_id",
		 vdid->vendor_id, vdid->device_id, pci_drv_sysfs_path);
	do_bash_cmd(switch_cmd_to_root(drv_path));

	remove_pci_dev(pci_id);
}

bool check_vfio_module(void)
{
	char *cmd = "modprobe 2>/dev/null vfio-pci; echo $?";
	char *present;

	present = do_bash_cmd(cmd);

	return !strtoul(present, &present, 10);
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

void bind_grp_modules(const char *pci_id, bool bind)
{
	struct list_item *list_modules;
	char path[MAX_LEN];

	snprintf(path, sizeof(path), "for line in $(ls %s%s/iommu_group/devices); do echo $line; done",
		 pci_dev_sysfs_path, pci_id);

	list_modules = do_bash_cmd_list(path);

	for (; list_modules != NULL; list_modules = list_modules->next) {
		if (bind)
			bind_vfio_module(list_modules->val, get_vdid(list_modules->val));
		else
			unbind_vfio_module(list_modules->val, get_vdid(list_modules->val));
	}

	if (!bind)
		do_pci_rescan();
}

/*int main(void)
{
	bind_grp_modules("0000:03:00.0", false);
	return 0;
}*/

