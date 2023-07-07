// SPDX-License-Identifier: LGPL-2.0

/*
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include "pciutils.h"

#define ICM_DRV_READY	0x3

/* Configuration spaces */
#define PATH_CFG	0
#define ADP_CFG		1
#define ROUTER_CFG	2
#define CNTR_CFG	3

/* EOF/SOF of read/write packets */
#define EOF_SOF_READ	1
#define EOF_SOF_WRITE	2

/* Transmit descriptor flags w.r.t. the position of 'flags' in the descriptor's memory */
#define TX_DESC_DONE	BIT(1)
#define TX_REQ_STS	BIT(2)
#define TX_INT_EN	BIT(3)

/* Receive descriptor flags w.r.t. the position of 'flags' in the descriptor's memory */
#define RX_DESC_DONE	BIT(1)
#define RX_BUF_OVF	BIT(2)
#define RX_REQ_STS	BIT(2)
#define RX_INT_EN	BIT(3)

/* Max. amount of time(us) taken by the router to write back into the host memory */
#define CTRL_TIMEOUT	2000

/* HopID and SuppID for control packets */
#define CTRL_HOP	0x0
#define CTRL_SUPP	0x0

#define TX_SIZE		16
#define RX_SIZE		16

struct req_payload {
	u32 addr:13;
	u32 len:6;
	u32 adp:6;
	u32 cfg_space:2;
	u32 seq_num:2;
	u32 rsvd:3;
};

struct tport_header {
	u32 hec:8;
	u32 len:8;
	u32 hop_id:11;
	u32 supp_id:1;
	u32 pdf:4;
};

struct read_req {
	u32 route_high;
	u32 route_low;
	struct req_payload payload;
	u32 crc;
};

struct write_req {
	u32 route_high;
	u32 route_low;
	struct req_payload payload;
	u32 buf;
	u32 crc;
};

/* Ring descriptor */
struct ring_desc {
	u32 addr_low;
	u32 addr_high;
	u32 len:12;
	u32 eof_pdf:4;
	u32 sof_pdf:4;
	u32 flags:12;
	u32 rsvd;
};
