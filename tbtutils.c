// SPDX-License-Identifier: LGPL-2.0
/*
 * Thunderbolt utilities
 *
 * This file provides various functionalities for the user on the thunderbolt IP,
 * including:
 * 1. Thunderbolt h/w initialization
 * 2. Host interface config. space access
 * 3. Dynamic allocation and mapping of DMA control packets as and when required
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "tbtutils.h"

#define TRIM_NUM_PATH		13

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

/* List of the descriptors for ring 0 of TX and RX */
static struct va_phy_addr tx_desc[TX_SIZE];
static struct va_phy_addr rx_desc[RX_SIZE];

/* Currently used descriptors */
static u8 tx_index = 0;
/* Unusable for now */
/*static u8 rx_index = 0;*/

/* A static page index to keep track of iova offset to be given */
static u64 page_index = 0;

/* Returns the total thunderbolt domains present in the system */
static u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_bash_cmd(path);

	return strtoud(output);
}

static void tx_index_inc(void)
{
	tx_index = (tx_index + 1) % TX_SIZE;
}

/* Unusable for now */
/*static void rx_index_inc(void)
{
	rx_index = (rx_index + 1) % RX_SIZE;
}*/

/* Unusable for now */
/*static struct tport_header* make_tport_header(u64 len, u8 pdf)
{
	struct tport_header *header = calloc(1, sizeof(struct tport_header));
	u32 *ptr;
	u8 crc;

	header->len = len;
	header->hop_id = CTRL_HOP;
	header->supp_id = CTRL_SUPP;
	header->pdf = pdf;

	ptr = (u32*)header;

	*ptr = htobe32(*ptr);
	crc = get_crc8(0, (u8*)header, sizeof(struct tport_header) - 1);
	*ptr = be32toh(*ptr);

	header->hec = crc;
	return header;
}*/

static struct req_payload* make_req_payload(u32 addr, u64 len, u32 adp, u32 cfg_space)
{
	struct req_payload *payload = calloc(1, sizeof(struct req_payload));

	payload->addr = addr;
	payload->len = len;
	payload->adp = adp;
	payload->cfg_space = cfg_space;

	return payload;
}

/* Prepare the transmit descriptor and the read buffer request */
static struct ring_desc* make_tx_read_req(const struct vfio_hlvl_params *params, u64 route,
					  const struct req_payload *payload,
					  struct vfio_iommu_type1_dma_map **dma_map)
{
	struct ring_desc *desc;
	struct read_req *req;

	/* DMA mapping for read control request */
	*dma_map = iommu_map_va(params->container, RDWR_FLAG, page_index++);

	desc = (struct ring_desc*)tx_desc[tx_index].va;
	desc->addr_low = (*dma_map)->iova & BITMASK(31, 0);
	desc->addr_high = ((*dma_map)->iova & BITMASK(63, 32)) >> 32;
	desc->len = sizeof(struct read_req);
	desc->eof_pdf = EOF_SOF_READ;
	desc->sof_pdf = EOF_SOF_READ;
	desc->flags = TX_REQ_STS;
	desc->rsvd = 0;

	req = (struct read_req*)(*dma_map)->vaddr;
	req->route_high = (route & BITMASK(63, 32)) >> 32;
	req->route_low = route & BITMASK(31, 0);
	req->payload = *payload;

	convert_to_be32((u32*)req, (sizeof(struct read_req) - 4) / 4);

	req->crc = htobe32(~(get_crc32(~0, (u8*)req, sizeof(struct read_req) - 4)));

	return desc;
}

/* Prepare the receive descriptor and the write buffer request */
/* Unusable for now */
/*static struct ring_desc* make_rx_read_resp(const struct vfio_hlvl_params *params)
{
	struct vfio_iommu_type1_dma_map *dma_map;
	struct ring_desc *desc;
	struct write_req *resp;

	dma_map = iommu_map_va(params->container, RDWR_FLAG, page_index++);

	desc = (struct ring_desc*)rx_desc[rx_index].va;
	memset(desc, sizeof(struct ring_desc), 0);

	desc->addr_low = dma_map->iova & BITMASK(31, 0);
	desc->addr_high = (dma_map->iova & BITMASK(63, 32)) >> 32;
	desc->flags = RX_REQ_STS;
	desc->rsvd = 0;

	resp = (struct write_req*)dma_map->vaddr;
	memset(resp, sizeof(struct write_req), 0);

	return desc;
}*/

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

	/* Increment the transmit descriptor index for future transactions */
	tx_index_inc();
}

/* Wait for the FW_ready bit to settle down */
static int tbt_wait_for_pwr(const char *pci_id)
{
	char *root_cmd, *bash_op;
	int ret = ETIMEDOUT;
	u16 retries = 350; /* TODO: Verify the proper amount of retries */
	char cmd[MAX_LEN];
	u32 val;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L", pci_id, VS_CAP_9);
	root_cmd = switch_cmd_to_root(cmd);

	while (retries--) {
		bash_op = do_bash_cmd(root_cmd);
		bash_op[8] = '\0';

		val = strtoud(bash_op);
		if (val & VS_FW_RDY) {
			printf("FW_RDY bit is set\n");
			ret = 0;

			free(bash_op);

			break;
		}

		free(bash_op);

		msleep(5);
	}

	free(root_cmd);

	return ret;
}

/* Load the required f/w from the IMR and power on the TBT IP */
static int tbt_hw_force_pwr(const char *pci_id, u32 val)
{
	char *root_cmd, *bash_op;
	char cmd[MAX_LEN];

	val &= VS_DMA_DELAY_MASK;
	val |= 0x22 < VS_DMA_DELAY_SHIFT; /* TODO: Check the DMA delay counter value */
	val |= VS_FORCE_PWR;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L=0x%x", pci_id, VS_CAP_22, val);
	root_cmd = switch_cmd_to_root(cmd);
	bash_op = do_bash_cmd(root_cmd);

	free(root_cmd);
	free(bash_op);

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

	val = strtoud(bash_op);
	val &= 0xffff;
	ltr = val << 16 | val; /* Always use the max. LTR value to be safe */

	free(root_cmd);
	free(bash_op);

	snprintf(wr_cmd, sizeof(wr_cmd), "setpci -s %s 0x%x.L=0x%x", pci_id, VS_CAP_15, ltr);
	root_cmd = switch_cmd_to_root(wr_cmd);
	bash_op = do_bash_cmd(root_cmd);

	free(root_cmd);
	free(bash_op);
}

/* Returns the host thunderbolt controller's PCI ID for the given domain */
char* trim_host_pci_id(u8 domain)
{
	char *pci_id = malloc(MAX_LEN * sizeof(char));
	char path[MAX_LEN];
	char *buf;
	u16 pos;

	if (total_domains() < (domain + 1)) {
		fprintf(stderr, "invalid domain\n");
		free(pci_id);

		return NULL;
	}

	snprintf(path, sizeof(path), "%s%d-0", tbt_sysfs_path, domain);

	buf = malloc(MAX_LEN * sizeof(char));
	readlink(path, buf, MAX_LEN);

	pos = strpos(buf, "domain", 0);
	pos -= TRIM_NUM_PATH;

	strncpy(pci_id, buf + pos, TRIM_NUM_PATH - 1);
	pci_id[TRIM_NUM_PATH] = '\0'; /*
				       * 'strncpy' doesn't guarantee a NULL-terminated
				       * string.
				       */

	free(buf);

	return trim_white_space(pci_id);
}

/* Reset the host-interface registers to their default values */
void reset_host_interface(const struct vfio_hlvl_params *params)
{
	write_host_mem(params, HOST_RESET, RESET);

	/* Host interface takes a max. of 10 ms to reset */
	msleep(10);
}

/* Allocate the TX descriptors and reserve the DMA memory */
void allocate_tx_desc(const struct vfio_hlvl_params *params)
{
	struct vfio_iommu_type1_dma_map *dma_map;
	u8 i = 0;

	printf("allocating and mapping %u DMA TX descriptors\n", TX_SIZE);
	for (; i < TX_SIZE; i++) {
		dma_map = iommu_map_va(params->container, RDWR_FLAG, page_index++);

		tx_desc[i].dma_map = dma_map;
		tx_desc[i].va = (void*)dma_map->vaddr;
		tx_desc[i].iova = dma_map->iova;
	}
}

/* Allocate the RX descriptors and reserve the DMA memory */
void allocate_rx_desc(const struct vfio_hlvl_params *params)
{
	struct vfio_iommu_type1_dma_map *dma_map;
	u8 i = 0;

	printf("allocating and mapping %u DMA RX descriptors\n", RX_SIZE);
	for (; i < RX_SIZE; i++) {
		dma_map = iommu_map_va(params->container, RDWR_FLAG, page_index++);

		rx_desc[i].dma_map = dma_map;
		rx_desc[i].va = (void*)dma_map->vaddr;
		rx_desc[i].iova = dma_map->iova;
	}
}

/*
 * Initialize the host-interface transmit registers.
 * Ring size is coded with a total of 16 descriptors, to make it equal to
 * the RX ring size.
 */
void init_host_tx(const struct vfio_hlvl_params *params)
{
	u32 val = 0;

	printf("initializing host-interface config. for TX\n");

	write_host_mem(params, TX_BASE_LOW, tx_desc[0].iova & BITMASK(31,0));
	write_host_mem(params, TX_BASE_HIGH, (tx_desc[0].iova & BITMASK(63, 32)) >> 32);
	write_host_mem(params, TX_PROD_CONS_INDEX, 0);
	write_host_mem(params, TX_RING_SIZE, TX_SIZE); /* Optimum no. of descriptors? */

	val |= TX_RAW | TX_VALID;
	write_host_mem(params, TX_RING_CTRL, val);
}

/*
 * Initialize the receive host-interface registers. Ring size has been set to 16 since
 * the CM spec. indicates min. size to be 256 bytes.
 * Data buffer size of the RX layer needs to be set with the no. of bytes to be posted in
 * the host memory. For now, program it to '0' to represent a max. of 4096 bytes.
 */
void init_host_rx(const struct vfio_hlvl_params *params)
{
	u32 val = 0;

	printf("initializing host-interface config. for RX\n");

	write_host_mem(params, RX_BASE_LOW, rx_desc[0].iova & BITMASK(31, 0));
	write_host_mem(params, RX_BASE_HIGH, (rx_desc[0].iova & BITMASK(63, 32)) >> 32);
	write_host_mem(params, RX_PROD_CONS_INDEX, 0);

	/*
	 * Optimum no. of descriptors: 256 (min. bytes required) / 16 (bytes in a
	 * descriptor).
	 * Correct buffer size?
	 */
	write_host_mem(params, RX_RING_BUF_SIZE, RX_SIZE);

	val |= RX_RAW | RX_VALID;
	write_host_mem(params, RX_RING_CTRL, val);
}

/*
 * Request router config. space of the router at the provided route for the given no.
 * of dwords.
 */
int request_router_cfg(const char *pci_id, const struct vfio_hlvl_params *params,
		       u64 route, u32 addr, u64 dwords)
{
	struct req_payload *payload = make_req_payload(addr, dwords, 0, ROUTER_CFG);
	struct vfio_iommu_type1_dma_map *dma_map = NULL;
	struct ring_desc *tx_desc;
	/* Not needed for transmission */
	//struct ring_desc *rx_desc = make_rx_read_resp(params);
	int ret = 0;

	tx_desc = make_tx_read_req(params, route, payload, &dma_map);

	allow_bus_master(pci_id);

	tx_start(params);
	usleep(CTRL_TIMEOUT);

	/*
	 * Host interface layer of the router will set the 'TX_DESC_DONE' flag in the
	 * TX descriptor stored in the host memory if successful transmission has
	 * occured. Hence, verify it via reading the flag.
	 */
	if (!(tx_desc->flags & TX_DESC_DONE)) {
		fprintf(stderr, "transport layer failed to receive the control packet\n");
		ret = 1;

		goto free;
	} else
		printf("read request successfully posted to the transport layer\n");

free:
	free(payload);
	free_dma_map(params->container, dma_map);

	return ret;
}

int tbt_hw_init(const char *pci_id)
{
	char *root_cmd, *bash_op;
	char cmd[MAX_LEN];
	int ret = 0;
	u32 val;

	snprintf(cmd, sizeof(cmd), "setpci -s %s 0x%x.L", pci_id, VS_CAP_22);
	root_cmd = switch_cmd_to_root(cmd);
	bash_op = do_bash_cmd(root_cmd);
	bash_op = trim_white_space(bash_op);

	val = strtouh(bash_op);
	ret = tbt_hw_force_pwr(pci_id, val);
	if (ret) {
		fprintf(stderr, "timeout in powering on the TBT h/w\n");
		goto out;
	}

	tbt_hw_set_ltr(pci_id);

out:
	free(root_cmd);
	free(bash_op);

	return ret;
}

/* Free the allocated DMA mapping of the descriptors */
void free_tx_rx_desc(const struct vfio_hlvl_params *params)
{
	u8 i = 0;

	for (; i < TX_SIZE; i++)
		free_dma_map(params->container, tx_desc[i].dma_map);

	for (i = 0; i < RX_SIZE; i++)
		free_dma_map(params->container, rx_desc[i].dma_map);
}
