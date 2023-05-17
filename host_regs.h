// SPDX-License-Identifier: GPL-3.0

// Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>

#include "utils.h"

/* Indication of the TBT HW loading the f/w from the IMR */
#define VS_CAP_9			0xc8
#define VS_FW_RDY			BIT(31)

/* Latency tolerance reporting */
#define VS_CAP_15			0xe0
#define VS_CAP_16			0xe4

/* PCIe config force power registers */
#define VS_CAP_22			0xfc
#define VS_FORCE_PWR			BIT(1)
#define VS_DMA_DELAY_MASK		BITMASK(31, 24)
#define VS_DMA_DELAY_SHIFT		24

/* ICM registers (used for thunderbolt initialization) */
#define ICM_FW_STS			0x39944
#define ICM_NVM_AUTH_DONE		BIT(31)
#define ICM_EN				BIT(0)
#define ICM_CIO_RST_REQ			BIT(30)
#define ICM_EN_CPU			BIT(2)
#define ICM_EN_INV			BIT(1)

#define HOST_CAPS			0x39640
#define TOTAL_PATHS			BITMASK(10, 0)

#define HOST_RESET			0x39858
#define RESET				BIT(0)

#define HOST_CTRL			0x39864
#define DISABLE_ISR_AUTO_CLR		BIT(17)

#define HOST_CL1_ENABLE			0x39880

#define HOST_CL2_ENABLE			0x39884

/* Only TX ring 0 is utilized in this library for simplicity */
#define TX_BASE_LOW			0x0
#define TX_BASE_HIGH			0x4

#define TX_PROD_CONS_INDEX		0x8
#define TX_PROD_INDEX			BITMASK(31, 16)
#define TX_PROD_INDEX_SHIFT		16

#define TX_RING_SIZE			0xc

#define TX_RING_CTRL			0x19800
#define TX_E2E_FLOW_EN			BIT(28)
#define TX_NS				BIT(29)
#define TX_RAW				BIT(30)
#define TX_VALID			BIT(31)

/* Only RX ring 0 is utilized in this library for simplicity */
#define RX_BASE_LOW			0x8000
#define RX_BASE_HIGH			0x8004

#define RX_PROD_CONS_INDEX		0x8008
#define RX_PROD_INDEX			BITMASK(31, 16)
#define RX_PROD_INDEX_SHIFT		16
#define RX_CONS_INDEX			BITMASK(15, 0)

#define RX_RING_BUF_SIZE		0x800c
#define RX_RING_SIZE			BITMASK(15, 0)
#define RX_RING_DATA_BUF_SIZE		BITMASK(27, 16)
#define RX_RING_BUF_SIZE_SHIFT		16

#define RX_RING_CTRL			0x29800
#define RX_TX_E2E_HOP_ID		BITMASK(22, 12)
#define RX_TX_E2E_HOP_ID_SHIFT		12
#define RX_E2E_FLOW_EN			BIT(28)
#define RX_NS				BIT(29)
#define RX_RAW				BIT(30)
#define RX_VALID			BIT(31)

#define RX_RING_PDF			0x29804
#define RX_EOF_PDF			BITMASK(15, 0)
#define RX_SOF_PDF			BITMASK(31, 16)
#define RX_SOF_PDF_MASK_SHIFT		16
