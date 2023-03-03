#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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
			write_host_mem(params, off, 12);

		off += 4;
	}

	val |= TX_RAW | TX_VALID;
	write_host_mem(params, TX_RING_CTRL, val);

	return dma_map->vaddr;
}

/*
 * Initialize the receive host-interface registers. Ring size has been set to '2' similar to
 * the transmit ring size.
 * 'Ring size' register of the RX layer needs to be set with the no. of bytes to be posted in
 * the host memory.
 */
static void* init_host_rx(const struct vfio_hlvl_params *params, u64 len)
{
	struct vfio_iommu_type1_dma_map *dma_map = iommu_map_va(params->container, RDWR_FLAG,
								page_index++);
	u32 val = 0;
	u64 off;

	off = RX_BASE_LOW;

	while (off <= RX_RING_BUF_SIZE) {
		if (off == RX_BASE_LOW)
			write_host_mem(params, off, dma_map->iova & BITMASK(31, 0));
		else if (off == RX_BASE_HIGH)
			write_host_mem(params, off, (dma_map->iova & BITMASK(63, 32)) >> 32);
		else if (off == RX_PROD_CONS_INDEX)
			write_host_mem(params, off, 0);
		else if (off == RX_RING_BUF_SIZE)
			write_host_mem(params, off, 12 | (24 << 16));

		off += 4;
	}

	val |= RX_RAW | RX_VALID;
	write_host_mem(params, RX_RING_CTRL, val);

	write_host_mem(params, RX_RING_PDF, 0);

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

	convert_to_be32(req, (sizeof(struct read_req) - 4) / 4);
	req->crc = ~(get_crc32(~0, req, sizeof(struct read_req) - 4));
	be32_to_u32(req, (sizeof(struct read_req) - 4) / 4);

	return desc;
}

/* Prepare the receive descriptor and the write buffer request */
static struct rx_pair* make_rx_read_req(const struct vfio_hlvl_params *params, const u64 route,
					const struct req_payload *payload, u64 len)
{
	struct vfio_iommu_type1_dma_map *dma_map;
	struct rx_desc *desc;
	struct write_req *req;
	struct rx_pair *pair;

	/* DMA mapping for control requests */
	dma_map = iommu_map_va(params->container, RDWR_FLAG, page_index++);

	desc = init_host_rx(params, 4 * len);
	desc->addr_low = dma_map->iova & BITMASK(31, 0);
	desc->addr_high = (dma_map->iova & BITMASK(63, 32)) >> 32;
	desc->flags = RX_REQ_STS;
	desc->rsvd = 0;

	req = dma_map->vaddr;
	/*req->route_high = (route & BITMASK(63, 32)) >> 32;
	req->route_low = route & BITMASK(31, 0);
	req->payload = *payload;
	req->buf = 0;
	req->crc = 0;*/
	memset(req, sizeof(struct write_req), 0);

	pair = malloc(sizeof(struct rx_pair));
	pair->desc = desc;
	pair->req = req;

	return pair;
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

/* Increases the RX consumer index by 1 to start a RX transmission */
static void rx_start(const struct vfio_hlvl_params *params)
{
	u16 cons_index, size;
	u32 index;

	index = read_host_mem_long(params, RX_PROD_CONS_INDEX);
	cons_index = index & BITMASK(15, 0);
	size = read_host_mem_word(params, RX_RING_BUF_SIZE);

	cons_index = (cons_index + 1) % size;

	index &= ~BITMASK(15, 0);
	index |= cons_index;
	write_host_mem(params, RX_PROD_CONS_INDEX, index);
}

/*
 * Reads 'len' no. of doublewords from the router config space, starting from 'addr'.
 * 'len' should be greater than '0' and less than '60'. The caller needs to ensure that.
 */
static u32 read_router_cfg(const char *pci_id,const struct vfio_hlvl_params *params,
			   const u64 route, const u32 addr, const u64 len)
{
	struct req_payload *payload = make_req_payload(addr, len, 0, ROUTER_CFG);
	struct tx_desc *tx_desc = make_tx_read_req(params, route, payload);
	struct rx_pair *rx_pair = make_rx_read_req(params, route, payload, len);

	allow_bus_master(pci_id);

	rx_start(params);
	tx_start(params);

	msleep(10000);

	if (!(tx_desc->flags & TX_DESC_DONE)) {
		printf("transport layer failed to receive the control packet\n");
		return NULL;
	}

	if (!(rx_pair->desc->flags & RX_DESC_DONE)) {
		printf("failed to write the buffer into the host memory\n");
		return NULL;
	}
}

/* Wait for the FW_ready bit to settle down */
static int tbt_wait_for_pwr(const char *pci_id)
{
	char *root_cmd, *bash_op;
	u16 retries = 350; /* TODO: Verify the proper amount of retries */
	char cmd[MAX_LEN];
	u32 val;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L", pci_id, VS_CAP_9);
	root_cmd = switch_cmd_to_root(cmd);

	while (retries--) {
		bash_op = do_bash_cmd(root_cmd);
		bash_op[8] = '\0';

		val = strtoul(bash_op, &bash_op, 16);
		if (val & VS_FW_RDY)
			return 0;

		msleep(5);
	}

	return ETIMEDOUT;
}

/* Load the required f/w from the IMR and power on the TBT IP */
static int tbt_hw_force_pwr(const char *pci_id, u32 val)
{
	char cmd[MAX_LEN];
	char *root_cmd;

	val &= VS_DMA_DELAY_MASK;
	val |= 0x22 < VS_DMA_DELAY_SHIFT; /* TODO: Check the DMA delay counter value */
	val |= VS_FORCE_PWR;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L=0x%x", pci_id, VS_CAP_22, val);
	root_cmd = switch_cmd_to_root(cmd);
	do_bash_cmd(root_cmd);

	return tbt_wait_for_pwr(pci_id);
}

static void tbt_hw_set_ltr(const char *pci_id)
{
	char cmd[MAX_LEN], wr_cmd[MAX_LEN];
	char *root_cmd, *bash_op;
	u32 val, ltr;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L", pci_id, VS_CAP_16);
	root_cmd = switch_cmd_to_root(cmd);
	bash_op = do_bash_cmd(root_cmd);
	bash_op[8] = '\0';

	val = strtoul(bash_op, &bash_op, 16);
	val &= 0xffff;
	ltr = val << 16 | val; /* Always use the max. LTR value to be safe */

	snprintf(wr_cmd, sizeof(wr_cmd), "setpci -s %s 0x%x.L=0x%x", pci_id, VS_CAP_15, ltr);
	root_cmd = switch_cmd_to_root(wr_cmd);
	do_bash_cmd(root_cmd);
}

static int tbt_hw_init(const char *pci_id)
{
	char *root_cmd, *bash_op;
	char cmd[MAX_LEN];
	u32 val;
	int ret;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L", pci_id, VS_CAP_22);
	root_cmd = switch_cmd_to_root(cmd);
	bash_op = do_bash_cmd(root_cmd);
	bash_op[8] = '\0';

	val = strtoul(bash_op, &bash_op, 16);
	ret = tbt_hw_force_pwr(pci_id, val);
	if (ret) {
		printf("timeout in powering on the TBT h/w\n");
		return ret;
	}

	tbt_hw_set_ltr(pci_id);

	return 0;
}

int main(void)
{
	/*bind_grp_modules("0000:00:0d.2", false);
	return 0;*/
	char *pci_id = "0000:00:0d.2";
	struct vdid *vdid = get_vdid(pci_id);
	printf("%s, %s\n", vdid->vendor_id, vdid->device_id);
	printf("%s\n", pci_id);
	u32 num = 0x4002000;
	printf("tobe:%llx\n", htobe32(num));

	u8 data[] = {0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 32, 0};
	u32 crc = get_crc32(~0, data, 12);
	printf("crc:%llx\n", crc);
	printf("negate crc:%llx\n", ~crc);
	printf("crcbe:%llx\n", htobe32(~crc));
	//return 0;
	//bind_grp_modules(pci_id, true);

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
	reset_host_interface(params);
	tbt_hw_init(pci_id);
	int i = 3;
	while(i--)
	read_router_cfg(pci_id, params, 0, 0, 1);

	printf("removing vfio\n");
	//bind_grp_modules(pci_id, false);
}
