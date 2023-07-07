// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

/* Router configuration registers related to USB4 */
#define ROUTER_CS_1				0x1
#define ROUTER_CS_1_UPS_ADP			BITMASK(13, 8)
#define ROUTER_CS_1_UPS_ADP_SHIFT		8
#define ROUTER_CS_1_MAX_ADP			BITMASK(19, 14)
#define ROUTER_CS_1_MAX_ADP_SHIFT		14
#define ROUTER_CS_1_DEPTH			BITMASK(22, 20)
#define ROUTER_CS_1_DEPTH_SHIFT			20
#define ROUTER_CS_1_REV_NO			BITMASK(31, 24)
#define ROUTER_CS_1_REV_NO_SHIFT		24

#define ROUTER_CS_2				0x2
#define ROUTER_CS_2_TOP_ID_LOW			BITMASK(31, 0)

#define ROUTER_CS_3				0x3
#define ROUTER_CS_3_TOP_ID_HIGH			BITMASK(23, 0)
#define ROUTER_CS_3_TOP_ID_VALID		BIT(31)

#define ROUTER_CS_4				0x4
#define ROUTER_CS_4_NOT_TIMEOUT			BITMASK(7, 0)
#define ROUTER_CS_4_CMUV			BITMASK(15, 8)
#define ROUTER_CS_4_CMUV_SHIFT			8
#define ROUTER_CS_4_USB4V			BITMASK(31, 24)
#define ROUTER_CS_4_USB4V_SHIFT			24
#define USB4V_MAJOR_VER				BITMASK(7, 5)
#define USB4V_MAJOR_VER_SHIFT			5

#define ROUTER_CS_5				0x5
#define ROUTER_CS_5_WOP				BIT(1)
#define ROUTER_CS_5_WOU				BIT(2)
#define ROUTER_CS_5_WOD				BIT(3)
#define ROUTER_CS_5_C3S				BIT(23)
#define ROUTER_CS_5_PTO				BIT(24)
#define ROUTER_CS_5_UTO				BIT(25)
#define ROUTER_CS_5_IHCO			BIT(26)
#define ROUTER_CS_5_CV				BIT(31)

#define ROUTER_CS_6				0x6
#define ROUTER_CS_6_SR				BIT(0)
#define ROUTER_CS_6_TNS				BIT(1)
#define ROUTER_CS_6_WOPS			BIT(2)
#define ROUTER_CS_6_WOUS			BIT(3)
#define ROUTER_CS_6_WODS			BIT(4)
#define ROUTER_CS_6_IHCI			BIT(18)
#define ROUTER_CS_6_RR				BIT(24)
#define ROUTER_CS_6_CR				BIT(25)

/* Router configuration registers related to TBT3 */
#define ROUTER_VCAP_ID				0x05

#define ROUTER_VSEC1_ID				0x01

#define ROUTER_VSEC1_1				0x1
#define ROUTER_VSEC1_1_PED			BITMASK(6, 3)
#define ROUTER_VSEC1_1_PED_LANE			BIT(3)

#define ROUTER_VSEC3_ID				0x03

#define ROUTER_VSEC4_ID				0x04

#define ROUTER_VSEC6_ID				0x06

/* VSEC6 common region */
#define ROUTER_VSEC6_COM			0x2
#define ROUTER_VSEC6_COM_PORTS			BITMASK(3, 0)
#define ROUTER_VSEC6_COM_LEN			BITMASK(15, 8)
#define ROUTER_VSEC6_COM_LEN_SHIFT		8
#define ROUTER_VSEC6_COM_USB4_LEN		BITMASK(27, 16)
#define ROUTER_VSEC6_COM_USB4_LEN_SHIFT		16

/* VSEC6 port mode */
#define ROUTER_VSEC6_PORT_MODE			0x26

/* VSEC6 port attributes */
#define ROUTER_VSEC6_PORT_ATTR			0x8d
#define ROUTER_VSEC6_PORT_ATTR_BE		BIT(12)

/* VSEC6 lane control */
#define ROUTER_VSEC6_LC_SX_CTRL			0x96
#define ROUTER_VSEC6_LC_SX_CTRL_EWE		BITMASK(10, 0)
#define ROUTER_VSEC6_LC_SX_CTRL_L0C		BIT(16)
#define ROUTER_VSEC6_LC_SX_CTRL_L1C		BIT(20)

/* VSEC6 link attributes */
#define ROUTER_VSEC6_LINK_ATTR			0x97
#define ROUTER_VSEC6_LINK_ATTR_TCM		BIT(17)
#define ROUTER_VSEC6_LINK_ATTR_CPS		BIT(18)

/*
 * Used to fetch the adapter numbers in each level the lower level is connected
 * to in a topology ID.
 * Check for route string format in control packets on how the topology ID is
 * stored.
 */
#define LEVEL_0			BITMASK(5, 0)
#define LEVEL_1			BITMASK(13, 8)
#define LEVEL_2			BITMASK(21, 16)
#define LEVEL_3			BITMASK(29, 24)
#define LEVEL_4			BITMASK(37, 32)
#define LEVEL_5			BITMASK(45, 40)
#define LEVEL_6			BITMASK(53, 48)

/*
 * Wake events configured in a TBT3 router.
 * Check for 'get_tbt3_wake_events_en' definition in 'router.c'.
 */
#define TBT3_HOT_PLUG_ROUTER	BIT(1)
#define TBT3_HOT_UNPLUG_ROUTER	BIT(2)
#define TBT3_HOT_PLUG_DP	BIT(3)
#define TBT3_HOT_UNPLUG_DP	BIT(4)
#define TBT3_USB4		BIT(5)
#define TBT3_PCIE		BIT(6)
#define TBT3_HOT_PLUG_USB	BIT(9)
#define TBT3_HOT_UNPLUG_USB	BIT(10)

char* get_route_string(u64 top_id);
u8 get_upstream_adp(const char *router);
u8 get_max_adp(const char *router);
u64 get_top_id_low(const char *router);
u64 get_top_id_high(const char *router);
u16 get_rev_no(const char *router);
u64 is_router_configured(const char *router);
u16 get_notification_timeout(const char *router);
u16 get_cmuv(const char *router);
u16 get_usb4v(const char *router);
u16 is_wake_enabled(const char *router, u8 protocol);
u64 is_tunneling_on(const char *router, u8 protocol);
u64 is_ihci_on(const char *router);
u64 is_tunneling_config_valid(const char *router);
u16 is_router_sleep_ready(const char *router);
u16 is_tbt3_not_supported(const char *router);
u16 get_wake_status(const char *router, u8 protocol);
u64 is_ihci_present(const char *router);
u64 is_router_ready(const char *router);
u64 is_tunneling_ready(const char *router);

/* Below functions are applicable only for TBT3 routers */
u16 is_tbt3_hot_events_disabled_lane(const char *router);
u16 get_tbt3_com_reg_dwords(const char *router);
u32 get_tbt3_usb4_reg_dwords(const char *router);
u16 get_tbt3_usb4_ports(const char *router);
u32 is_tbt3_bonding_en(const char *router, u8 port);
u32 get_tbt3_wake_events_en(const char *router, u8 port);
u64 get_tbt3_lanes_configured(const char *router, u8 port);
u64 is_tbt3_compatible_mode(const char *router, u8 port);
u64 is_tbt3_clx_supported(const char *router, u8 port);

u8 get_usb4_port_num(u8 lane_adp);
