#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pciutils.h"

#define TRIM_NUM_PATH		13

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

static u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_bash_cmd(path);

	return strtoul(output, &output, 10);
}

static char* trim_host_pci_id(const u8 domain)
{
	char *pci_id = malloc(MAX_LEN * sizeof(char));
	char path[MAX_LEN];
	u16 pos;

	if (total_domains() < (domain + 1)) {
		printf("invalid domain\n");
		return NULL;
	}

	snprintf(path, sizeof(path), "%s%d-0", tbt_sysfs_path, domain);

	readlink(path, path, MAX_LEN);

	pos = strpos(path, "domain", 0);
	pos -= TRIM_NUM_PATH;

	strncpy(pci_id, path + pos, TRIM_NUM_PATH - 1);
	pci_id[TRIM_NUM_PATH - 1] = '\0';

	return pci_id;
}

/* Reset the host-interface registers to their default values */
/*static void reset_host_interface(struct vfio_hlvl_params *params)
{
	write_host_mem(params, HOST_RESET, RESET);*/

	/* Host interface takes a max. of 10 ms to reset */
	/*usleep(10000);
}*/

/*
 * Initialize the host-interface trasmit registers.
 * This library works on only one ring descriptor at once hence initialize the size of the ring
 * to 2. This is done so we could let the producer and consumer indexes become different for
 * the transmission to get processed by the host-interface layer.
 */
/*static void* init_host_tx(const struct vfio_hlvl_params *params)
{
	struct vfio_iommu_type1_dma_map *dma_map = iommu_map_va(params->container, RDWR_FLAG, 0);
	u32 val = 0;
	u64 off;

	off = TX_BASE_LOW;

	while (off <= TX_RING_SIZE) {
		if (off == TX_BASE_LOW)
			write_host_mem(params, off, dma_map->iova & BITMASK(31,0));
		else if (off == TX_BASE_HIGH)
			write_host_mem(params, off, (dma_map->iova & BITMASK(63, 32)) >> 32);
		else if (off == TX_PROD_CONS_INDEX)
			write_host_mem(params, off, 0);
		else if (off == TX_RING_SIZE)
			write_host_mem(params, off, 2);

		off += 4;
	}

	val |= TX_RAW | TX_VALID;
	write_host_mem(params, TX_RING_CTRL, val);

	return dma_map->vaddr;
}

static struct tx_desc* make_tx_desc(const struct vfio_hlvl_params *params, const u64 size)
{
	struct vfio_iommu_type1_dma_map *dma_map;
	struct tx_desc *desc;
	u32 tx_ctrl = 0;

	desc = init_host_tx(params);
	desc = malloc(sizeof(struct tx_desc));

	tx_ctrl = size;
}*/

int main(void)
{
	/*char *pci_id = trim_host_pci_id(0);
	struct vdid *vdid = get_vdid(pci_id);
	printf("%s, %s\n", vdid->vendor_id, vdid->device_id);*/

	u32 num = 0x4002000;
	printf("tobe:%llx\n", htobe32(num));

	u8 data[] = {0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 32, 0};
	u32 crc = get_crc32(~0, data, 12);
	printf("crc:%llx\n", crc);
	printf("negate crc:%llx\n", ~crc);
	printf("crcbe:%llx\n", htobe32(~crc));
	return 0;

/*	bind_grp_modules(pci_id, true);

	struct vfio_hlvl_params *params = vfio_dev_init(pci_id);
        printf("%d %d %d %d\n", params->container, params->group, params->device, params->dev_info->num_regions);
	get_dev_bar_regions(params, pci_id);
	get_dev_pci_cfg_region(params, pci_id);
	struct vfio_region_info *find_bar = find_bar_for_off(params->bar_regions, 0x0);
	printf("bar:%d\n", find_bar->index);
	printf("%d\n",get_page_aligned_addr(4096));

	printf("mem:0x%x\n", read_host_mem_byte(params, 0x39880));
	write_host_mem(params, 0x39880, 0x0);
	printf("mem:0x%x\n", read_host_mem_byte(params, 0x39880));

	printf("mask:%x\n", BITMASK(3,1));

	struct vfio_iommu_type1_dma_map *map =	iommu_map_va(params->container, READ_FLAG, 0);
	printf("%x %x\n", map->iova, map->size);

	iommu_unmap_va(params->container, map);
	return 0;
	map =  iommu_map_va(params->container, WRITE_FLAG, 1);
        printf("%x %x\n", map->iova, map->size);

        iommu_unmap_va(params->container, map);

	reset_host_interface(params);
	init_host_tx(params);*/
}
