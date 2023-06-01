// SPDX-License-Identifier: GPL-3.0
/*
 * Abstractions for VFIO-related functionalities
 *
 * This file provides helper functions to work with the VFIO parameters
 * included in '<linux/vfio.h>'.
 * Much of the work is handled in this file including initialization,
 * access (reads/writes), etc.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

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

/* Binds the VFIO module to the provided PCI device */
static void bind_vfio_module(const char *pci_id, const struct vdid *vdid)
{
	char dev_path[MAX_LEN];
	char drv_path[3 * MAX_LEN];

	snprintf(dev_path, sizeof(dev_path), "echo %s > %s%s/driver/unbind",
		 pci_id, pci_dev_sysfs_path, pci_id);
	do_bash_cmd(switch_cmd_to_root(dev_path));

	snprintf(drv_path, sizeof(drv_path), "echo '%s %s' > %svfio-pci/new_id",
		 vdid->vendor_id, vdid->device_id, pci_drv_sysfs_path);
	do_bash_cmd(switch_cmd_to_root(drv_path));
}

/* Unbinds the VFIO module from the provided PCI device and removes it */
static void unbind_vfio_module(const char *pci_id, const struct vdid *vdid)
{
	char dev_path[MAX_LEN];
	char drv_path[3 * MAX_LEN];

	snprintf(dev_path, sizeof(dev_path), "echo %s > %s%s/driver/unbind",
		 pci_id, pci_dev_sysfs_path, pci_id);
	do_bash_cmd(switch_cmd_to_root(dev_path));

	snprintf(drv_path, sizeof(drv_path), "echo '%s %s' > %svfio-pci/remove_id",
		 vdid->vendor_id, vdid->device_id, pci_drv_sysfs_path);
	do_bash_cmd(switch_cmd_to_root(drv_path));

	remove_pci_dev(pci_id);
}

/* Returns the IOMMU group number of the provided PCI device */
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
	return trim_white_space(iommu_grp);
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

static bool is_region_mmap(struct vfio_region_info *reg_info)
{
	return reg_info->flags & VFIO_REGION_INFO_FLAG_MMAP;
}

/* Returns one dword from the host interface config. space at the given offset */
static u32 read_host_mem(const struct vfio_hlvl_params *params, u64 off)
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

/* Not usable for now */
/*static struct vfio_iommu_type1_info* get_iommu_info(int container)
{
	struct vfio_iommu_type1_info *info = malloc(sizeof(struct vfio_iommu_type1_info));

	info->argsz = sizeof(struct vfio_iommu_type1_info);

	ioctl(container, VFIO_IOMMU_GET_INFO, info);

	return info;
}*/

bool check_vfio_module(void)
{
	char *cmd = "modprobe 2>/dev/null vfio-pci; echo $?";
	char *present;

	present = do_bash_cmd(cmd);

	return !strtoud(present);
}

/*
 * Binds VFIO to all the modules present in the IOMMU group of the given PCI device and
 * returns the list of all the such modules.
 */
struct pci_vdid* bind_grp_modules(const char *pci_id)
{
	struct list_item *list_modules;
	struct pci_vdid *dev_list;
	char path[MAX_LEN];
	u64 i = 0;

	snprintf(path, sizeof(path), "for line in $(ls %s%s/iommu_group/devices); do echo $line; done",
		 pci_dev_sysfs_path, pci_id);

	list_modules = do_bash_cmd_list(path);
	dev_list = malloc(get_total_list_items(list_modules) * sizeof(struct pci_vdid));

	for (; list_modules != NULL; list_modules = list_modules->next) {
		dev_list[i].pci_id = (char*)list_modules->val;
		dev_list[i++].vdid = get_vdid(list_modules->val);

		bind_vfio_module((char*)list_modules->val, get_vdid(list_modules->val));
	}

	return dev_list;
}

/* Unbinds VFIO to all the PCI modules present in the 'dev_list' provided */
void unbind_grp_modules(struct pci_vdid *dev_list, u64 num)
{
	u64 i = 0;

	for (; i < num; i++)
		unbind_vfio_module(dev_list[i].pci_id, dev_list[i].vdid);

	do_pci_rescan();
}

/* Initialize the VFIO for the given PCI ID */
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
		fprintf(stderr, "unknown API version\n");
		return NULL;
	}

	if (!ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
		fprintf(stderr, "type-1 iommu not supported\n");
		return NULL;
	}

	iommu_grp = find_iommu_grp(pci_id);
	snprintf(path, sizeof(path), "/dev/vfio/%s", iommu_grp);

	group = open(path, O_RDWR);
	ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		fprintf(stderr, "group not viable\n");
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
	*params->dev_info = device_info;

	return params;
}

/* Returns the BAR regions of the given PCI device */
void get_dev_bar_regions(struct vfio_hlvl_params *params)
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

/* Returns the PCI config. space region of the given PCI device */
void get_dev_pci_cfg_region(struct vfio_hlvl_params *params)
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

/* Returns the BAR region number for the given offset */
struct vfio_region_info* find_bar_for_off(struct list_item *bar_regions, u64 off)
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
		fprintf(stderr, "offset:0x%" PRIx64 " out of bounds\n", off);
		return NULL;
	}

	return (struct vfio_region_info*)(temp->val);
}

u32 read_host_mem_long(const struct vfio_hlvl_params *params, u64 off)
{
	return read_host_mem(params, off);
}

u16 read_host_mem_word(const struct vfio_hlvl_params *params, u64 off)
{
	return read_host_mem(params, off) & BITMASK(15, 0);
}

u8 read_host_mem_byte(const struct vfio_hlvl_params *params, u64 off)
{
	return read_host_mem(params, off) & BITMASK(7, 0);
}

/* Writes a dword to the host interface config. space at the given offset */
void write_host_mem(const struct vfio_hlvl_params *params, u64 off, u32 value)
{
	struct vfio_region_info *reg_info = find_bar_for_off(params->bar_regions, off);
	struct list_item *temp = params->bar_regions;
	struct vfio_region_info *temp_reg;
	u64 prev_size = 0;
	void *user_va;

	if (!is_region_mmap(reg_info))
		return;

	while (temp->val != reg_info) {
		temp_reg = (struct vfio_region_info*)(temp->val);
		prev_size += temp_reg->size;

		temp = temp->next;
	}

	user_va = get_user_mapped_write_va(params->device, reg_info->offset, reg_info->size);
	*(u32*)(user_va + (off - prev_size)) = value;

	unmap_user_mapped_va(user_va, reg_info->size);
}

/*
 * Prepare a VFIO DMA mapping for the given container.
 *
 * Note: Since all the storage classes for thunderbolt hardware are less than page
 * size and aligment requirement for the mapping is in multiples of page size, prepare
 * the mapping for a whole page, and not for a specific size for ease.
 */
struct vfio_iommu_type1_dma_map* iommu_map_va(int container, u8 op_flags, u8 index)
{
	struct vfio_iommu_type1_info info = { .argsz = sizeof(info) };
	struct vfio_iommu_type1_dma_map *dma_map;
	u64 pgsize_sup;

	dma_map = malloc(sizeof(struct vfio_iommu_type1_dma_map));
	dma_map->argsz = sizeof(struct vfio_iommu_type1_dma_map);

	ioctl(container, VFIO_IOMMU_GET_INFO, &info);
	pgsize_sup = get_size_least_set(info.iova_pgsizes);

	if (op_flags == READ_FLAG) {
		dma_map->vaddr = (u64)get_user_mapped_read_va(-1, 0, pgsize_sup);
		dma_map->flags = VFIO_DMA_MAP_FLAG_READ;
	} else if (op_flags == WRITE_FLAG) {
		dma_map->vaddr = (u64)get_user_mapped_write_va(-1, 0, pgsize_sup);
		dma_map->flags = VFIO_DMA_MAP_FLAG_WRITE;
	} else if (op_flags == RDWR_FLAG) {
		dma_map->vaddr = (u64)get_user_mapped_rw_va(-1, 0, pgsize_sup);
		dma_map->flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;
	}

	dma_map->iova = index * pgsize_sup;
	dma_map->size = pgsize_sup;

	ioctl(container, VFIO_IOMMU_MAP_DMA, dma_map);

	return dma_map;
}

/* Destroy the DMA mapping created */
void iommu_unmap_va(int container, const struct vfio_iommu_type1_dma_map *dma_map)
{
	struct vfio_iommu_type1_dma_unmap dma_unmap = { .argsz = sizeof(dma_unmap) };

	dma_unmap.size = dma_map->size;
	dma_unmap.iova = dma_map->iova;

	ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
}
