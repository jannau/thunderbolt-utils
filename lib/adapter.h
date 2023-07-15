// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

/* Basic adapter configuration space */
#define ADP_CS_2				0x2
#define ADP_CS_2_SUB_TYPE			BITMASK(7, 0)
#define ADP_CS_2_VERSION			BITMASK(15, 8)
#define ADP_CS_2_PROTOCOL			BITMASK(23, 16)
#define ADP_CS_2_PVS				BITMASK(23, 0)

#define ADP_CS_3				0x3
#define ADP_CS_3_NUM				BITMASK(25, 20)

#define ADP_CS_4				0x4
#define ADP_CS_4_PLUGGED			BIT(30)
#define ADP_CS_4_LOCK				BIT(31)

#define ADP_CS_5				0x5
#define ADP_CS_5_DHP				BIT(31)

/*
 * Protocol, version, and sub-type fields of the respective adapter.
 * This represents bits [23:0] of the common 'ADP_CS_2' register.
 */
#define UNSUPPORTED_PVS				0x000000
#define LANE_PVS				0x000001
#define HOST_INTERFACE_PVS			0x000002
#define DOWN_PCIE_PVS				0x100101
#define UP_PCIE_PVS				0x100102
#define DP_OUT_PVS				0x0e0102
#define DP_IN_PVS				0x0e0101
#define DOWN_USB3_PVS				0x200101
#define UP_USB3_PVS				0x200102

/* Identifiers for the adapters present in a router */
#define LANE_NUM				0x0
#define HOST_INTERFACE_NUM			0x1
#define DOWN_PCIE_NUM				0x2
#define UP_PCIE_NUM				0x3
#define DP_OUT_NUM				0x4
#define DP_IN_NUM				0x5
#define DOWN_USB3_NUM				0x6
#define UP_USB3_NUM				0x7

/* Lane adapter configuration space */
#define LANE_ADP_CS_0				0x0
#define LANE_ADP_CS_0_SUP_SPEEDS		BITMASK(19, 16) /* Same for Lane-1 as well */
#define LANE_ADP_CS_0_SUP_SPEEDS_SHIFT		16
#define LANE_ADP_CS_0_SUP_WIDTH			BITMASK(25, 20)
#define LANE_ADP_CS_0_SUP_WIDTH_SHIFT		20
#define LANE_ADP_CS_0_CL0S_SUP			BIT(26)
#define LANE_ADP_CS_0_CL1_SUP			BIT(27)
#define LANE_ADP_CS_0_CL2_SUP			BIT(28)

#define LANE_ADP_CS_1				0x1
#define LANE_ADP_CS_1_CL0S_EN			BIT(10)
#define LANE_ADP_CS_1_CL1_EN			BIT(11)
#define LANE_ADP_CS_1_CL2_EN			BIT(12)
#define LANE_ADP_CS_1_LD			BIT(14)
#define LANE_ADP_CS_1_LB			BIT(15)
#define LANE_ADP_CS_1_CUR_LINK_SPEED		BITMASK(19, 16) /* Same for Lane-1 as well */
#define LANE_ADP_CS_1_CUR_LINK_SPEED_SHIFT	16
#define LANE_ADP_CS_1_NEG_LINK_WIDTH		BITMASK(25, 20) /* Same for Lane-1 as well */
#define LANE_ADP_CS_1_NEG_LINK_WIDTH_SHIFT	20
#define LANE_ADP_CS_1_ADP_STATE			BITMASK(29, 26)
#define LANE_ADP_CS_1_ADP_STATE_SHIFT		26
#define LANE_ADP_CS_1_PMS			BIT(30)

#define LANE_ADP_CS_2				0x2
#define LANE_ADP_CS_2_LLEE			BITMASK(22, 16)

#define LANE_SPEED_GEN3				BIT(2)
#define LANE_SPEED_GEN2				BIT(3)

#define LANE_WIDTH_X1				BIT(0)
#define LANE_WIDTH_X2				BIT(1)

#define LANE_ADP_STATE_DISABLED			0x0
#define LANE_ADP_STATE_TRAINING			0x1
#define LANE_ADP_STATE_CL0			0x2
#define LANE_ADP_STATE_TRANS_CL0S		0x3
#define LANE_ADP_STATE_RECEIVE_CL0S		0x4
#define LANE_ADP_STATE_CL1			0x5
#define LANE_ADP_STATE_CL2			0x6
#define LANE_ADP_STATE_CLD			0x7

#define LANE_ADP_CAP_ID				0x1

/* USB4 port configuration space (only for Lane-0 adapter) */
#define PORT_CS_18				0x12
#define PORT_CS_18_CUSB4_VER			BITMASK(7, 0)
#define PORT_CS_18_CUSB4_VER_MAJ		BITMASK(7, 4)
#define PORT_CS_18_CUSB4_VER_MAJ_SHIFT		4
#define PORT_CS_18_BE				BIT(8)
#define PORT_CS_18_TCM				BIT(9)
#define PORT_CS_18_CPS				BIT(10)
#define PORT_CS_18_RD				BIT(13)
#define PORT_CS_18_WOCS				BIT(16)
#define PORT_CS_18_WODS				BIT(17)
#define PORT_CS_18_WOU4S			BIT(18)

#define PORT_CS_19				0x13
#define PORT_CS_19_DPR				BIT(0)
#define PORT_CS_19_PC				BIT(3)
#define PORT_CS_19_EWOC				BIT(16)
#define PORT_CS_19_EWOD				BIT(17)
#define PORT_CS_19_EWOU4			BIT(18)

#define CABLE_VER_MAJ_TBT3			0x0
#define CABLE_VER_MAJ_USB4			0x1

#define USB4_PORT_CAP_ID			0x6

/* USB3 configuration space */
#define ADP_USB3_CS_0				0x0
#define ADP_USB3_CS_0_VALID			BIT(30)
#define ADP_USB3_CS_0_PE			BIT(31)

#define ADP_USB3_CS_1				0x1
#define ADP_USB3_CS_1_CUB			BITMASK(11, 0)
#define ADP_USB3_CS_1_CDB			BITMASK(23, 12)
#define ADP_USB3_CS_1_CDB_SHIFT			12
#define ADP_USB3_CS_1_HCA			BIT(31)

#define ADP_USB3_CS_2				0x2
#define ADP_USB3_CS_2_AUB			BITMASK(11, 0)
#define ADP_USB3_CS_2_ADB			BITMASK(23, 12)
#define ADP_USB3_CS_2_ADB_SHIFT			12
#define ADP_USB3_CS_2_CMR			BIT(31)

#define ADP_USB3_CS_3				0x3
#define ADP_USB3_CS_3_SCALE			BITMASK(5, 0)

#define ADP_USB3_CS_4				0x4
#define ADP_USB3_CS_4_ALR			BITMASK(6, 0)
#define ADP_USB3_CS_4_ULV			BIT(7)
#define ADP_USB3_CS_4_PLS			BITMASK(11, 8)
#define ADP_USB3_CS_4_PLS_SHIFT			8
#define ADP_USB3_CS_4_MAX_SUP_LR		BITMASK(18, 12)
#define ADP_USB3_CS_4_MAX_SUP_LR_SHIFT		12

#define USB3_LR_GEN2_SL				0x0 /* 10 Gbps (single-lane) */
#define USB3_LR_GEN2_DL				0x1 /* 20 Gbps (dual-lane) */

#define USB3_PLS_U0				0x0
#define USB3_PLS_U2				0x2
#define USB3_PLS_U3				0x3
#define USB3_PLS_DISABLED			0x4
#define USB3_PLS_RX_DETECT			0x5
#define USB3_PLS_INACTIVE			0x6
#define USB3_PLS_POLLING			0x7
#define USB3_PLS_RECOVERY			0x8
#define USB3_PLS_HOT_RESET			0x9
#define USB3_PLS_RESUME				0xf

#define USB3_ADP_CAP_ID				0x4
#define USB3_ADP_SEC_ID				0x0 /*
						     * Identification including PCIe and DP
						     * adapters which the have same capability
						     * ID. Also see, 'PCIE_ADP_SEC_ID' and
						     * 'DP_ADP_SEC_ID'.
						     */

/* PCIe configuration space */
#define ADP_PCIE_CS_0				0x0
#define ADP_PCIE_CS_0_LINK			BIT(16)
#define ADP_PCIE_CS_0_TX_EI			BIT(17)
#define ADP_PCIE_CS_0_RX_EI			BIT(18)
#define ADP_PCIE_CS_0_RST			BIT(19)
#define ADP_PCIE_CS_0_LTSSM			BITMASK(28, 25)
#define ADP_PCIE_CS_0_LTSSM_SHIFT		25
#define ADP_PCIE_CS_0_PE			BIT(31)

#define PCIE_LTSSM_DETECT			0x0
#define PCIE_LTSSM_POLLING			0x1
#define PCIE_LTSSM_CONFIGURATION		0x2
#define PCIE_LTSSM_CONFIGURATION_IDLE		0x3
#define PCIE_LTSSM_RECOVERY			0x4
#define PCIE_LTSSM_RECOVERY_IDLE		0x5
#define PCIE_LTSSM_L0				0x6
#define PCIE_LTSSM_L1				0x7
#define PCIE_LTSSM_L2				0x8
#define PCIE_LTSSM_DISABLED			0x9
#define PCIE_LTSSM_HOT_RESET			0xa

#define PCIE_ADP_CAP_ID				0x4
#define PCIE_ADP_SEC_ID				0x1

/* DP configuration space */
#define ADP_DP_CS_0				0x0
#define ADP_DP_CS_0_AE				BIT(30)
#define ADP_DP_CS_0_VE				BIT(31)
#define ADP_DP_CS_2				0x2
#define ADP_DP_CS_2_NRD_MLC			BITMASK(2, 0) /* Only for DP IN adapters */
#define ADP_DP_CS_2_HPD				BIT(6)
#define ADP_DP_CS_2_NRD_MLR			BITMASK(9, 7) /* Only for DP IN adapters */
#define ADP_DP_CS_2_NRD_MLR_SHIFT		7
#define ADP_DP_CS_2_CA				BIT(10) /* Only for DP IN adapters */
#define ADP_DP_CS_2_GR				BITMASK(12, 11) /* Only for DP IN adapters */
#define ADP_DP_CS_2_GR_SHIFT			11
#define ADP_DP_CS_2_CMMS			BIT(20) /* Only for DP IN adapters */
#define ADP_DP_CS_2_EBW				BITMASK(31, 24) /* Only for DP IN adapters */
#define ADP_DP_CS_2_EBW_SHIFT			24

/* DP common local capabilities configuration space */
#define DP_LOCAL_CAP				0x4
#define DP_LOCAL_CAP_IN_BW_ALLOC_SUP		BIT(28) /* Only for DP IN adapters */

/* DP common remote capabilities configuration space */
#define DP_REMOTE_CAP				0x5

/* Common for local, remote, and common capabilities */
#define DP_CAP_PAV				BITMASK(3, 0)
#define DP_CAP_MLR				BITMASK(11, 8)
#define DP_CAP_MLR_SHIFT			8
#define DP_CAP_MLC				BITMASK(14, 12)
#define DP_CAP_MLC_SHIFT			12
#define DP_CAP_MST				BIT(15)
#define DP_CAP_LTTPR				BIT(27)
#define DP_CAP_DSC				BIT(29)

/* Only for DP IN adapters */
#define DP_STATUS				0x6
#define DP_STATUS_LC				BITMASK(2, 0)
#define DP_STATUS_LR				BITMASK(11, 8)
#define DP_STATUS_LR_SHIFT			8
#define DP_STATUS_ABW				BITMASK(31, 24)
#define DP_STATUS_ABW_SHIFT			24

/* Only for DP OUT adapters */
#define DP_STATUS_CTRL				0x6
#define DP_STATUS_CTRL_LC			BITMASK(2, 0)
#define DP_STATUS_CTRL_LR			BITMASK(11, 8)
#define DP_STATUS_CTRL_LR_SHIFT			8
#define DP_STATUS_CTRL_CMHS			BIT(25)
#define DP_STATUS_CTRL_UF			BIT(26)

/* Common DP capability space */
#define DP_COMMON_CAP				0x7
#define DP_COMMON_CAP_DPRX_CRD			BIT(31) /* Only for DP IN adapters */

/* Only for DP IN adapters */
#define ADP_DP_CS_8				0x8
#define ADP_DP_CS_8_RBW				BITMASK(7, 0)
#define ADP_DP_CS_8_DPME			BIT(30)
#define ADP_DP_CS_8_DR				BIT(31)

#define DP_ADP_CAP_ID				0x4
#define DP_ADP_SEC_ID				0x2

#define DP_IN_BW_GR_QUARTER			0x0 /* 0.25 Gbps */
#define DP_IN_BW_GR_HALF			0x1 /* 0.5 Gbps */
#define DP_IN_BW_GR_FULL			0x2 /* 1.0 Gbps */

#define DP_USB4_SPEC_TBT3			0x3
#define DP_USB4_SPEC_USB4_1			0x4

#define DP_ADP_LR_RBR				0x0 /* 1.62 GHz */
#define DP_ADP_LR_HBR				0x1 /* 2.7 GHz */
#define DP_ADP_LR_HBR2				0x2 /* 5.4 GHz */
#define DP_ADP_LR_HBR3				0x3 /* 8.1 GHz */

#define DP_ADP_MAX_LC_X1			0x0 /* 1 lane */
#define DP_ADP_MAX_LC_X2			0x1 /* 2 lanes */
#define DP_ADP_MAX_LC_X4			0x2 /* 4 lanes */

#define DP_IN_ADP_LC_X1				0x1 /* 1 lane */
#define DP_IN_ADP_LC_X2				0x2 /* 2 lanes */
#define DP_IN_ADP_LC_X4				0x4 /* 4 lanes */

#define DP_OUT_ADP_LC_X0			0x0 /* DP main link is inactive */
#define DP_OUT_ADP_LC_X1			0x1 /* 1 lane */
#define DP_OUT_ADP_LC_X2			0x2 /* 2 lanes */
#define DP_OUT_ADP_LC_X4			0x4 /* 4 lanes */

extern bool are_adp_types_filled;

void fill_adp_types_in_router(const char *router);
u64 get_adp_pvs(const char *router, u8 adp);
u64 is_adp_plugged(const char *router, u8 adp);
u64 is_adp_locked(const char *router, u8 adp);
u64 are_hot_events_disabled(const char *router, u8 adp);
bool is_adp_lane(const char *router, u8 adp);
u16 get_sup_link_speeds(const char *router, u8 adp);
u16 get_sup_link_widths(const char *router, u8 adp);
u64 are_cl0s_supported(const char *router, u8 adp);
u64 is_cl1_supported(const char *router, u8 adp);
u64 is_cl2_supported(const char *router, u8 adp);
u32 are_cl0s_enabled(const char *router, u8 adp);
u32 is_cl1_enabled(const char *router, u8 adp);
u32 is_cl2_enabled(const char *router, u8 adp);
u32 is_lane_disabled(const char *router, u8 adp);
u16 cur_link_speed(const char *router, u8 adp);
u16 neg_link_width(const char *router, u8 adp);
u16 get_lane_adp_state(const char *router, u8 adp);
u64 is_secondary_lane_adp(const char *router, u8 adp);
bool is_adp_lane_0(const char *router, u8 adp);
u16 get_usb4_cable_version(const char *router, u8 adp);
u32 is_usb4_bonding_en(const char *router, u8 adp);
u32 is_usb4_tbt3_compatible_mode(const char *router, u8 adp);
u32 is_usb4_clx_supported(const char *router, u8 adp);
u32 is_usb4_router_detected(const char *router, u8 adp);
u64 get_usb4_wake_status(const char *router, u8 adp);
u16 is_usb4_port_configured(const char *router, u8 adp);
u64 get_usb4_wakes_en(const char *router, u8 adp);
bool is_adp_up_usb3(const char *router, u8 adp);
bool is_adp_down_usb3(const char *router, u8 adp);
bool is_adp_usb3(const char *router, u8 adp);
u64 is_usb3_adp_en(const char *router, u8 adp);
u32 get_usb3_consumed_up_bw(const char *router, u8 adp);
u32 get_usb3_consumed_down_bw(const char *router, u8 adp);
u32 get_usb3_allocated_up_bw(const char *router, u8 adp);
u32 get_usb3_allocated_down_bw(const char *router, u8 adp);
u16 get_usb3_scale(const char *router, u8 adp);
u16 get_usb3_actual_lr(const char *router, u8 adp);
u16 is_usb3_link_valid(const char *router, u8 adp);
u16 get_usb3_port_link_state(const char *router, u8 adp);
u16 get_usb3_max_sup_lr(const char *router, u8 adp);
bool is_adp_up_pcie(const char *router, u8 adp);
bool is_adp_down_pcie(const char *router, u8 adp);
bool is_adp_pcie(const char *router, u8 adp);
u64 is_pcie_link_up(const char *router, u8 adp);
u64 is_pcie_tx_ei(const char *router, u8 adp);
u64 is_pcie_rx_ei(const char *router, u8 adp);
u64 is_pcie_switch_warm_reset(const char *router, u8 adp);
u16 get_pcie_ltssm(const char *router, u8 adp);
u64 is_pcie_adp_enabled(const char *router, u8 adp);
bool is_adp_dp_in(const char *router, u8 adp);
bool is_adp_dp_out(const char *router, u8 adp);
bool is_adp_dp(const char *router, u8 adp);
u64 is_dp_aux_en(const char *router, u8 adp);
u64 is_dp_vid_en(const char *router, u8 adp);
u16 get_dp_in_nrd_max_lc(const char *router, u8 adp);
u16 get_dp_hpd_status(const char *router, u8 adp);
u16 get_dp_in_nrd_max_lr(const char *router, u8 adp);
u16 is_dp_in_cm_ack(const char *router, u8 adp);
u16 get_dp_in_granularity(const char *router, u8 adp);
u64 is_dp_in_cm_bw_alloc_support(const char *router, u8 adp);
u16 get_dp_in_estimated_bw(const char *router, u8 adp);
u16 get_dp_protocol_adp_ver(const char *router, u8 adp, bool remote);
u16 get_dp_max_link_rate(const char *router, u8 adp, bool remote);
u16 get_dp_max_lane_count(const char *router, u8 adp, bool remote);
u32 is_dp_mst_cap(const char *router, u8 adp, bool remote);
u64 is_dp_lttpr_sup(const char *router, u8 adp, bool remote);
u64 is_dp_in_bw_alloc_sup(const char *router, u8 adp);
u64 is_dp_dsc_sup(const char *router, u8 adp, bool remote);
u16 get_dp_in_lane_count(const char *router, u8 adp);
u16 get_dp_in_link_rate(const char *router, u8 adp);
u16 get_dp_in_alloc_bw(const char *router, u8 adp);
u16 get_dp_out_lane_count(const char *router, u8 adp);
u16 get_dp_out_link_rate(const char *router, u8 adp);
u64 is_dp_out_cm_handshake(const char *router, u8 adp);
u64 is_dp_out_dp_in_usb4(const char *router, u8 adp);
u64 is_dp_in_dprx_cap_read_done(const char *router, u8 adp);
u16 get_dp_in_req_bw(const char *router, u8 adp);
u64 is_dp_in_dptx_bw_alloc_en(const char *router, u8 adp);
u64 is_dp_in_dptx_req(const char *router, u8 adp);
