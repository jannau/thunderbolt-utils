#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "tb_cfg.h"

#define TRIM_NUM_PATH	13

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

/* A static page index to keep track of iova offset to be given */
static page_index = 0;

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
static void reset_host_interface(struct vfio_hlvl_params *params)
{
	write_host_mem(params, HOST_RESET, RESET);

	/* Host interface takes a max. of 10 ms to reset */
	usleep(10000);
}

/*
 * Initialize the host-interface transmit registers.
 * This library works on only one ring descriptor at once hence initialize the size of the ring
 * to 2. This is done so we could let the producer and consumer indexes become different for
 * the transmission to get processed by the host-interface layer.
 */
static void* init_host_tx(const struct vfio_hlvl_params *params)
{
	struct vfio_iommu_type1_dma_map *dma_map = iommu_map_va(params->container, RDWR_FLAG,
								page_index++);
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

static struct req_payload* make_req_payload(const u32 addr, const u64 len, const u32 adp,
					    const u32 cfg_space)
{
	struct req_payload *payload = calloc(1, sizeof(struct req_payload));

	payload->addr = addr;
	payload->len = len;
	payload->adp = adp;
	payload->cfg_space = cfg_space;

	return payload;
}

/* Prepare the transmit descriptor and the read buffer request */
static struct tx_desc* make_tx_read_req(const struct vfio_hlvl_params *params, const u64 route,
					const struct req_payload *payload)
{
	struct vfio_iommu_type1_dma_map *dma_map;
	struct tx_desc *desc;
	struct read_req *req;

	/* DMA mapping for control requests */
	dma_map = iommu_map_va(params->container, RDWR_FLAG, page_index++);

	desc = init_host_tx(params);
	desc->addr_low = dma_map->iova & BITMASK(31, 0);
	desc->addr_high = (dma_map->iova & BITMASK(63, 32)) >> 32;
	desc->len = sizeof(struct read_req) - 4;
	desc->eof_pdf = EOF_SOF_READ;
	desc->sof_pdf = EOF_SOF_READ;
	desc->flags = TX_REQ_STS;
	desc->rsvd = 0;

	req = dma_map->vaddr;
	req->route_high = (route & BITMASK(63, 32)) >> 32;
	req->route_low = route & BITMASK(31, 0);
	req->payload = *payload;

	convert_to_be32(req, sizeof(struct read_req) - 4);
	req->crc = htobe32(~(get_crc32(~0, req, sizeof(struct read_req) - 4)));

	return desc;
}

/* Increases the TX producer index by 1 to start a TX transmission */
static void tx_start(const struct vfio_hlvl_params *params)
{
	u16 prod_index, cons_index, size;
	u32 val;

	prod_index = (read_host_mem_long(params, TX_PROD_CONS_INDEX) & BITMASK(31, 16)) >> 16;
	cons_index = read_host_mem_word(params, TX_PROD_CONS_INDEX);
	size = read_host_mem_word(params, TX_RING_SIZE);

	prod_index = (prod_index + 1) % size;

	val = (prod_index << 16) | cons_index;
	write_host_mem(params, TX_PROD_CONS_INDEX, val);
}

static u32 read_router_cfg(const char *pci_id,const struct vfio_hlvl_params *params,
			   const u64 route, const u32 addr)
{
	struct req_payload *payload = make_req_payload(addr, 1, 0, ROUTER_CFG);
	struct tx_desc *desc = make_tx_read_req(params, route, payload);

	allow_bus_master(pci_id);
	tx_start(params);
}

int main(void)
{
	char *pci_id = trim_host_pci_id(0);
	struct vdid *vdid = get_vdid(pci_id);
	printf("%s, %s\n", vdid->vendor_id, vdid->device_id);

	u32 num = 0x4002000;
	printf("tobe:%llx\n", htobe32(num));

	u8 data[] = {0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 32, 0};
	u32 crc = get_crc32(~0, data, 12);
	printf("crc:%llx\n", crc);
	printf("negate crc:%llx\n", ~crc);
	printf("crcbe:%llx\n", htobe32(~crc));

	bind_grp_modules(pci_id, true);

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

	/*struct vfio_iommu_type1_dma_map *map =	iommu_map_va(params->container, READ_FLAG, 0);
	printf("%x %x\n", map->iova, map->size);

	iommu_unmap_va(params->container, map);

	map =  iommu_map_va(params->container, WRITE_FLAG, 1);
        printf("%x %x\n", map->iova, map->size);

        iommu_unmap_va(params->container, map);

	reset_host_interface(params);
	init_host_tx(params);
	iommu_unmap_va(params->container, map);*/
	printf("after init:%x\n", read_host_mem_long(params, 0x19800));

	read_router_cfg(pci_id, params, 0, 0);
}
