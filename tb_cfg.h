#include "pciutils.h"

/* Configuration spaces */
#define PATH_CFG	0
#define ADP_CFG		1
#define ROUTER_CFG	2
#define CNTR_CFG	3

/* EOF/SOF of read/write packets */
#define EOF_SOF_READ	1
#define EOF_SOF_WRITE	2

/* Transmit descriptor flags w.r.t. the position of flags in the descriptor's memory */
#define TX_DESC_DONE	BIT(1)
#define TX_REQ_STS	BIT(2)
#define TX_INT_EN	BIT(3)

struct req_payload {
	u32 addr:13;
	u32 len:6;
	u32 adp:6;
	u32 cfg_space:2;
	u32 seq_num:2;
	u32 rsvd:3;
};

struct read_req {
	u32 route_high;
	u32 route_low;
	struct req_payload payload;
	u32 crc;
};

struct tx_desc {
	u32 addr_low;
	u32 addr_high;
	u32 len:12;
	u32 eof_pdf:4;
	u32 sof_pdf:4;
	u32 flags:12;
	u32 rsvd;
};
