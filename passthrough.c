#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "pciutils.h"

#define TRIM_IOMMU_NUM_PATH	13

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

static char* find_iommu_grp(const char *pci_id)
{
	char *iommu_grp = malloc(MAX_LEN * sizeof(char));
	char path[MAX_LEN];
	char *bash_result;
	u16 pos, len_str;

	snprintf(path, sizeof(path), "readlink %s%s/iommu_group", pci_dev_sysfs_path,
		 pci_id);

	bash_result = do_bash_cmd(path);

	pos = strpos(bash_result, "iommu_group", 0);
	len_str = strlen(bash_result + pos + TRIM_IOMMU_NUM_PATH);

	strncpy(iommu_grp, bash_result + pos + TRIM_IOMMU_NUM_PATH, len_str);
	iommu_grp[len_str] = '\0';
	return iommu_grp;
}

static bool is_vfio_bar_index(u8 index)
{
	if ((index == VFIO_PCI_BAR0_REGION_INDEX) || (index == VFIO_PCI_BAR1_REGION_INDEX) ||
	    (index == VFIO_PCI_BAR2_REGION_INDEX) || (index == VFIO_PCI_BAR3_REGION_INDEX) ||
	    (index == VFIO_PCI_BAR4_REGION_INDEX) || (index == VFIO_PCI_BAR5_REGION_INDEX))
		return true;

	return false;
}

static bool is_vfio_pci_cfg_index(u8 index)
{
	return index == VFIO_PCI_CONFIG_REGION_INDEX;
}

static bool is_region_mmap(const struct vfio_region_info *reg_info)
{
	return reg_info->flags & VFIO_REGION_INFO_FLAG_MMAP;
}


static u32 read_host_mem(const struct vfio_hlvl_params *params, const u64 off)
{
	struct vfio_region_info *reg_info = find_bar_for_off(params->bar_regions, off);
	struct list_item *temp = params->bar_regions;
	struct vfio_region_info *temp_reg;
	u64 prev_size = 0;
	void *user_va;
	u32 mem;

	if (!is_region_mmap(reg_info))
		return ~0;

	while (temp->val != reg_info) {
		temp_reg = (struct vfio_region_info*)(temp->val);
		prev_size += temp_reg->size;

		temp = temp->next;
	}

	user_va = get_user_mapped_read_va(params->device, reg_info->offset, reg_info->size);

	mem = *(u32*)(user_va + (off - prev_size));

	unmap_user_mapped_va(user_va, reg_info->size);

	return mem;
}

bool check_vfio_module(void)
{
	char *cmd = "modprobe 2>/dev/null vfio-pci; echo $?";
	char *present;

	present = do_bash_cmd(cmd);

	return !strtoul(present, &present, 10);
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
			bind_vfio_module((char*)list_modules->val, get_vdid(list_modules->val));
		else
			unbind_vfio_module((char*)list_modules->val, get_vdid(list_modules->val));
	}

	if (!bind)
		do_pci_rescan();
}

struct vfio_hlvl_params* vfio_dev_init(const char *pci_id)
{
	struct vfio_group_status group_status = { .argsz = sizeof(group_status) };
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	struct vfio_hlvl_params *params;
	int container, group, device;
	char path[MAX_LEN];
	char *iommu_grp;

	container = open("/dev/vfio/vfio", O_RDWR);
	if (ioctl(container, VFIO_GET_API_VERSION) != VFIO_API_VERSION) {
		printf("unknown API version\n");
		return NULL;
	}

	if (!ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
		printf("type-1 iommu not supported\n");
		return NULL;
	}

	iommu_grp = find_iommu_grp(pci_id);
	snprintf(path, sizeof(path), "/dev/vfio/%s", iommu_grp);

	group = open(path, O_RDWR);
	ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		printf("group not viable\n");
		return NULL;
	}

	device = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);

	ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

	device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, pci_id);
	ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);

	params = malloc(sizeof(struct vfio_hlvl_params));
	params->container = container;
	params->group = group;
	params->device = device;
	params->dev_info = &device_info;

	return params;
}

void get_dev_bar_regions(struct vfio_hlvl_params *params, const char *pci_id)
{
	struct list_item *temp = NULL;
	u8 i = 0;

	params->bar_regions = NULL;

	for (i = 0; i < VFIO_PCI_NUM_REGIONS; i++) {
		struct vfio_region_info *region_info = malloc(sizeof(struct vfio_region_info));

		region_info->argsz = sizeof(*region_info);

		if (!is_vfio_bar_index(i))
			continue;

		region_info->index = i;
		ioctl(params->device, VFIO_DEVICE_GET_REGION_INFO, region_info);

		if (region_info->size == 0x0)
			continue;

		temp = list_add(temp, (void*)region_info);

		if (!params->bar_regions)
			params->bar_regions = temp;
	}
}

void get_dev_pci_cfg_region(struct vfio_hlvl_params *params, const char *pci_id)
{
	u8 i = 0;

	params->pci_cfg_region = NULL;

	for (i = 0; i < VFIO_PCI_NUM_REGIONS; i++) {
		struct vfio_region_info *region_info = malloc(sizeof(struct vfio_region_info));

		region_info->argsz = sizeof(*region_info);

		if (!is_vfio_pci_cfg_index(i))
			continue;

		region_info->index = i;
		ioctl(params->device, VFIO_DEVICE_GET_REGION_INFO, region_info);

		params->pci_cfg_region = region_info;

		return;
	}
}

struct vfio_region_info* find_bar_for_off(const struct list_item *bar_regions, const u64 off)
{
	struct list_item *temp = bar_regions;
	struct vfio_region_info reg_info;
	u64 temp_size = 0;

	while (temp) {
		reg_info = *(struct vfio_region_info*)(temp->val);

		temp_size += reg_info.size;
		if (temp_size <= off) {
			temp = temp->next;
			continue;
		}

		break;
	}

	if (!temp) {
		printf("offset:0x%" PRIx64 " out of bounds\n", off);
		return NULL;
	}

	return (struct vfio_region_info*)(temp->val);
}

u32 read_host_mem_long(const struct vfio_hlvl_params *params, const u64 off)
{
	return read_host_mem(params, off);
}

u16 read_host_mem_word(const struct vfio_hlvl_params *params, const u64 off)
{
	return (u16)read_host_mem(params, off);
}

u8 read_host_mem_byte(const struct vfio_hlvl_params *params, const u64 off)
{
	return (u8)read_host_mem(params, off);
}

void write_host_mem(const struct vfio_hlvl_params *params, const u64 off, const u32 value)
{
	struct vfio_region_info *reg_info = find_bar_for_off(params->bar_regions, off);
	struct list_item *temp = params->bar_regions;
	struct vfio_region_info *temp_reg;
	u64 prev_size = 0;
	void *user_va;

	if (!is_region_mmap(reg_info))
		return ~0;

	while (temp->val != reg_info) {
		temp_reg = (struct vfio_region_info*)(temp->val);
		prev_size += temp_reg->size;

		temp = temp->next;
	}

	user_va = get_user_mapped_write_va(params->device, reg_info->offset, reg_info->size);
	*(u32*)(user_va + (off - prev_size)) = value;

	unmap_user_mapped_va(user_va, reg_info->size);
}
