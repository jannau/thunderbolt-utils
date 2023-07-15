// SPDX-License-Identifier: GPL-2.0
/*
 * Adapter configuration space
 *
 * This file provides functions to fetch the adapter config. space of a
 * TBT/USB4 device.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <string.h>
#include <stdio.h>

#include "helpers.h"

static u16 adp_types[MAX_ADAPTERS]; /* Hold an array to fill the adapter
				     * types for the adapters present in the router
				     * to enable faster access to the information
				     * regarding if an adapter is a specific protocol
				     * adapter or a lane adapter, or none.
				     */

bool are_adp_types_filled = false;

/* Function used to fill the above data structure */
void fill_adp_types_in_router(const char *router)
{
	u8 i = 1;

	adp_types[0] = MAX_BIT8;

	for (; i < MAX_ADAPTERS; i++) {
		if (is_adp_lane(router, i))
			adp_types[i] = LANE_NUM;
		else if (is_adp_up_usb3(router, i))
			adp_types[i] = UP_USB3_NUM;
		else if (is_adp_down_usb3(router, i))
			adp_types[i] = DOWN_USB3_NUM;
		else if (is_adp_up_pcie(router, i))
			adp_types[i] = UP_PCIE_NUM;
		else if (is_adp_down_pcie(router, i))
			adp_types[i] = DOWN_PCIE_NUM;
		else if (is_adp_dp_in(router, i))
			adp_types[i] = DP_IN_NUM;
		else if (is_adp_dp_out(router, i))
			adp_types[i] = DP_OUT_NUM;
		else
			adp_types[i] = MAX_BIT8;
	}

	are_adp_types_filled = true;
}

/*
 * Returns the protocol, version, and sub-type of the adapter.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 2^32.
 */
u64 get_adp_pvs(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, 0, 0, adp, ADP_CS_2);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_CS_2_PVS;
}

/*
 * Returns a positive integer if the adapter is plugged, '0' otherwise.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 2^32.
 */
u64 is_adp_plugged(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, 0, 0, adp, ADP_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_CS_4_PLUGGED;
}

/*
 * Returns a positive integer if control packets can't be forwarded via this
 * adapter, '0' otherwise.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 2^32.
 */
u64 is_adp_locked(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, 0, 0, adp, ADP_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_CS_4_LOCK;
}

/*
 * Returns a positive integer if hot events are disabled on this adapter,
 * '0' otherwise.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 2^32.
 */
u64 are_hot_events_disabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, 0, 0, adp, ADP_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_CS_5_DHP;
}

/*
 * Returns 'true' if the adapter is a lane adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_lane(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == LANE_NUM;

	pvs = get_adp_pvs(router, adp);

	if (pvs == MAX_BIT32)
		return false;

	return pvs == LANE_PVS;
}

/*
 * Fetch supported link speeds on the given lane adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'LANE_SPEED_GENX' definitions in 'adapter.h'.
 */
u16 get_sup_link_speeds(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_SUP_SPEEDS) >> LANE_ADP_CS_0_SUP_SPEEDS_SHIFT;
}

/*
 * Fetch supported link widths on the given lane adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'LANE_WIDTH_XX' definitions in 'adapter.h'.
 */
u16 get_sup_link_widths(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_SUP_WIDTH) >> LANE_ADP_CS_0_SUP_WIDTH_SHIFT;
}

/*
 * Returns a positive integer if CL0s are supported on the given lane, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 are_cl0s_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & LANE_ADP_CS_0_CL0S_SUP;
}

/*
 * Returns a positive integer if CL1 is supported on the given lane, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_cl1_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & LANE_ADP_CS_0_CL1_SUP;
}

/*
 * Returns a positive integer if CL2 is supported on the given lane, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_cl2_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & LANE_ADP_CS_0_CL2_SUP;
}

/*
 * Returns a positive integer if CL0s are enabled on the given lane, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 are_cl0s_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & LANE_ADP_CS_1_CL0S_EN;
}

/*
 * Returns a positive integer if CL1 is enabled on the given lane, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_cl1_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & LANE_ADP_CS_1_CL1_EN;
}

/*
 * Returns a positive integer if CL2 is enabled on the given lane, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_cl2_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & LANE_ADP_CS_1_CL2_EN;
}

/*
 * Returns a positive integer if the lane is disabled, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_lane_disabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & LANE_ADP_CS_1_LD;
}

/*
 * Fetch current link speeds on the given lane adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'LANE_SPEED_GENX' definitions in 'adapter.h'.
 */
u16 cur_link_speed(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_CUR_LINK_SPEED) >> LANE_ADP_CS_1_CUR_LINK_SPEED_SHIFT;
}

/*
 * Fetch negotiated link widths on the given lane adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'LANE_WIDTH_XX' definitions in 'adapter.h'.
 */
u16 neg_link_width(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_NEG_LINK_WIDTH) >> LANE_ADP_CS_1_NEG_LINK_WIDTH_SHIFT;
}

/*
 * Fetch the lane adapter state.
 * Return a value of 256 on any error.
 *
 * Check the 'LANE_ADP_STATE_X' definitions in 'adapter.h'.
 */
u16 get_lane_adp_state(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_ADP_STATE) >> LANE_ADP_CS_1_ADP_STATE_SHIFT;
}

/*
 * Returns a positive integer if the lane adapter is PM secondary, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_secondary_lane_adp(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & LANE_ADP_CS_1_PMS;
}


/*
 * Returns 'true' if the given adapter is Lane-0 adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_lane_0(const char *router, u8 adp)
{
	if (!is_adp_lane(router, adp))
		return false;

	return (adp % 2) != 0;
}

/*
 * Returns the USB4 version supported by the type-C cable plugged in.
 * Return a value of 256 on any error.
 *
 * Check the 'CABLE_VER_MAJ_X' definitions in 'adapter.h'._
 */
u16 get_usb4_cable_version(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & PORT_CS_18_CUSB4_VER_MAJ) >> PORT_CS_18_CUSB4_VER_MAJ_SHIFT;
}

/*
 * Returns a positive integer if conditions for lane bonding are met on the respective
 * port, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_usb4_bonding_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & PORT_CS_18_BE;
}

/*
 * Returns a positive integer if the link is operating in TBT3 mode, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_usb4_tbt3_compatible_mode(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & PORT_CS_18_TCM;
}

/*
 * Returns a positive integer if CLx is supported on the lane, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_usb4_clx_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & PORT_CS_18_CPS;
}

/*
 * Returns a positive integer if a router is detected on the port, '0' otherwise.
 * Return a value of 2^16 on any error.
 */
u32 is_usb4_router_detected(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT16;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & PORT_CS_18_RD;
}

/*
 * Returns the wake status on the USB4 port.
 * Return a value of 2^32 on any error.
 *
 * Check the 'PORT_CS_18_WX' defintions in 'adapter.h'.
 */
u64 get_usb4_wake_status(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val;
}

/*
 * Returns a positive integer if the USB4 port is configured, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_usb4_port_configured(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_19);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & PORT_CS_19_PC;
}

/*
 * Returns the wake events enabled on the USB4 port.
 * Return a value of 2^32 on any error.
 *
 * Check the 'PORT_CS_19_EX' definitions in 'adapter.h'.
 */
u64 get_usb4_wakes_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_19);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val;
}

/*
 * Returns 'true' if the adapter is an upstream USB3 adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_up_usb3(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == UP_USB3_NUM;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == UP_USB3_PVS;
}

/*
 * Returns 'true' if the adapter is a downstream USB3 adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_down_usb3(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == DOWN_USB3_NUM;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == DOWN_USB3_PVS;
}

/*
 * Returns 'true' if the adapter is a USB3 adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_usb3(const char *router, u8 adp)
{
	return is_adp_up_usb3(router, adp) || is_adp_down_usb3(router, adp);
}

/*
 * Returns a positive integer if the USB3 adapter is enabled, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_usb3_adp_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & (ADP_USB3_CS_0_VALID | ADP_USB3_CS_0_PE);
}

/*
 * Returns the consumed upstream bandwidth for USB3 traffic.
 * Return a value of 2^16 on any error.
 */
u32 get_usb3_consumed_up_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT16;

	if (!is_host_router(router))
		return 0;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & ADP_USB3_CS_1_CUB;
}

/*
 * Returns the consumed downstream bandwidth for USB3 traffic.
 * Return a value of 2^16 on any error.
 */
u32 get_usb3_consumed_down_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT16;

	if (!is_host_router(router))
		return 0;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return (val & ADP_USB3_CS_1_CDB) >> ADP_USB3_CS_1_CDB_SHIFT;
}

/*
 * Returns the allocated upstream bandwidth for USB3 traffic.
 * Return a value of 2^16 on any error.
 */
u32 get_usb3_allocated_up_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT16;

	if (!is_host_router(router))
		return 0;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & ADP_USB3_CS_2_AUB;
}

/*
 * Returns the allocated downstream bandwidth for USB3 traffic.
 * Return a value of 2^16 on any error.
 */
u32 get_usb3_allocated_down_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT16;

	if (!is_host_router(router))
		return 0;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return (val & ADP_USB3_CS_2_ADB) >> ADP_USB3_CS_2_ADB_SHIFT;
}

/*
 * Returns the granularity of USB3 bandwidth on the adapter.
 * Return a value of 256 on any error.
 */
u16 get_usb3_scale(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT8;

	if (!is_host_router(router))
		return 0;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_3);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_USB3_CS_3_SCALE;
}

/*
 * Returns the actual USB3 link rate.
 * Return a value of 256 on any error.
 *
 * Check the 'USB3_LR_X' definitions in 'adapter.h'.
 */
u16 get_usb3_actual_lr(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_USB3_CS_4_ALR;
}

/*
 * Returns a positive integer if the USB3 link is valid, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_usb3_link_valid(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_USB3_CS_4_ULV;
}

/*
 * Returns the USB3 port link state.
 * Return a value of 256 on any error.
 *
 * Check the 'USB3_PLS_X' definitions in 'adapter.h'.
 */
u16 get_usb3_port_link_state(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_USB3_CS_4_PLS) >> ADP_USB3_CS_4_PLS_SHIFT;
}

/*
 * Returns the max. supported USB3 link rate on the port.
 * Return a value of 256 on any error.
 *
 * Check the 'USB3_LR_X' definitions in 'adapter.h'.
 */
u16 get_usb3_max_sup_lr(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_usb3(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, USB3_ADP_CAP_ID, USB3_ADP_SEC_ID, adp, ADP_USB3_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_USB3_CS_4_MAX_SUP_LR) >> ADP_USB3_CS_4_MAX_SUP_LR_SHIFT;
}

/*
 * Returns 'true' if the adapter is an upstream PCIe adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_up_pcie(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == UP_PCIE_NUM;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == UP_PCIE_PVS;
}

/*
 * Returns 'true' if the adapter is a downstream PCIe adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_down_pcie(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == DOWN_PCIE_NUM;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == DOWN_PCIE_PVS;
}

/*
 * Returns 'true' if the adapter is a PCIe adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_pcie(const char *router, u8 adp)
{
	return is_adp_up_pcie(router, adp) || is_adp_down_pcie(router, adp);
}

/*
 * Returns a positive integer if PCIe PHY layer is active, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_pcie_link_up(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_pcie(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, PCIE_ADP_CAP_ID, PCIE_ADP_SEC_ID, adp, ADP_PCIE_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_PCIE_CS_0_LINK;
}

/*
 * Returns a positive integer if PCIe PHY TX is in electrical idle state, '0'
 * otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_pcie_tx_ei(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_pcie(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, PCIE_ADP_CAP_ID, PCIE_ADP_SEC_ID, adp, ADP_PCIE_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_PCIE_CS_0_TX_EI;
}

/*
 * Returns a positive integer if PCIe PHY RX is in electrical idle state, '0'
 * otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_pcie_rx_ei(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_pcie(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, PCIE_ADP_CAP_ID, PCIE_ADP_SEC_ID, adp, ADP_PCIE_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_PCIE_CS_0_RX_EI;
}

/*
 * Returns a positive integer if the attached PCIe switch port is in warm reset,
 * '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_pcie_switch_warm_reset(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_pcie(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, PCIE_ADP_CAP_ID, PCIE_ADP_SEC_ID, adp, ADP_PCIE_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_PCIE_CS_0_RST;
}

/*
 * Returns the PCIe PHY LTSSM state.
 * Return a value of 256 on any error.
 *
 * Check the 'PCIE_LTSSM_X' definitions in 'adapter.h'.
 */
u16 get_pcie_ltssm(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_pcie(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, PCIE_ADP_CAP_ID, PCIE_ADP_SEC_ID, adp, ADP_PCIE_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_PCIE_CS_0_LTSSM) >> ADP_PCIE_CS_0_LTSSM_SHIFT;
}

/*
 * Returns a positive integer if the PCIe adapter is enabled to send tunneled
 * packets, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_pcie_adp_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_pcie(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, PCIE_ADP_CAP_ID, PCIE_ADP_SEC_ID, adp, ADP_PCIE_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_PCIE_CS_0_PE;
}

/*
 * Returns 'true' if the adapter is a DP IN adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_dp_in(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == DP_IN_NUM;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == DP_IN_PVS;
}

/*
 * Returns 'true' if the adapter is a DP OUT adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_dp_out(const char *router, u8 adp)
{
	u64 pvs;

	if (are_adp_types_filled)
		return adp_types[adp] == DP_OUT_NUM;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == DP_OUT_PVS;
}

/*
 * Returns 'true' if the adapter is a DP adapter, 'false' otherwise.
 * Return 'false' on any error.
 */
bool is_adp_dp(const char *router, u8 adp)
{
	return is_adp_dp_in(router, adp) || is_adp_dp_out(router, adp);
}

/*
 * Returns a positive integer if AUX path is enabled on the DP adapter, '0'
 * otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_aux_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_DP_CS_0_AE;
}

/*
 * Returns a positive integer if video path is enabled on the DP adapter, '0'
 * otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_vid_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_DP_CS_0_VE;
}

/*
 * Returns the highest common max. lane count b/w two adapters, regardless of
 * bandwidth availability.
 * Return a value of 256 on any error.
 */
u16 get_dp_in_nrd_max_lc(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_DP_CS_2_NRD_MLC;
}

/*
 * Returns a positive integer if HPD is set in the DP adapter, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 get_dp_hpd_status(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_DP_CS_2_HPD;
}

/*
 * Returns the highest common max. link rate b/w two adapters, regardless of
 * bandwidth availability.
 * Return a value of 256 on any error.
 */
u16 get_dp_in_nrd_max_lr(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_DP_CS_2_NRD_MLR) >> ADP_DP_CS_2_NRD_MLR_SHIFT;
}

/*
 * Returns a positive integer if bandwidth allocation is done by the CM, '0'
 * otherwise.
 * Return a value of 256 on any error.
 */
u16 is_dp_in_cm_ack(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_DP_CS_2_CA;
}

/*
 * Get the bandwidth granularity for the DP adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'DP_IN_BW_GR_X' definitions in 'adapter.h'.
 */
u16 get_dp_in_granularity(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_DP_CS_2_GR) >> ADP_DP_CS_2_GR_SHIFT;
}

/*
 * Returns a positive integer if CM bandwidth allocation is supported on the
 * DP adapter, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_in_cm_bw_alloc_support(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_DP_CS_2_CMMS;
}

/*
 * Get the estimated available bandwidth indicated by the CM for the DP
 * adapter.
 * Return a value of 256 on any error.
 */
u16 get_dp_in_estimated_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_DP_CS_2_EBW) >> ADP_DP_CS_2_EBW_SHIFT;
}

/*
 * Get the USB4 spec. version the DP adapter supports.
 * Return a value of 256 on any error.
 *
 * @remote: 'true' if the access is for the remote capability.
 *
 * Check the 'DP_USB4_SPEC_X' definitions in 'adapter.h'.
 */
u16 get_dp_protocol_adp_ver(const char *router, u8 adp, bool remote)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp(router, adp))
		return MAX_BIT8;

	if (!remote)
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	else
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_REMOTE_CAP);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & DP_CAP_PAV;
}

/*
 * Get the max. link rate of the DP adapter.
 * Return a value of 256 on any error.
 *
 * @remote: 'true' if the access is for the remote capability.
 *
 * Check the 'DP_ADP_LR_X' definitions in 'adapter.h'.
 */
u16 get_dp_max_link_rate(const char *router, u8 adp, bool remote)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp(router, adp))
		return MAX_BIT8;

	if (!remote)
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	else
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_REMOTE_CAP);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & DP_CAP_MLR) >> DP_CAP_MLR_SHIFT;
}

/*
 * Get the max. lane count of the DP adapter.
 * Return a value of 256 on any error.
 *
 * @remote: 'true' if the access is for the remote capability.
 *
 * Check the 'DP_ADP_MAX_LC_XX' definitions in 'adapter.h'.
 */
u16 get_dp_max_lane_count(const char *router, u8 adp, bool remote)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp(router, adp))
		return MAX_BIT8;

	if (!remote)
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	else
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_REMOTE_CAP);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & DP_CAP_MLC) >> DP_CAP_MLC_SHIFT;
}

/*
 * Returns a positive integer if MST capability is supported on the DP
 * adapter, '0' otherwise.
 * Return a value of 2^16 on any error.
 *
 * @remote: 'true' if the access is for the remote capability.
 */
u32 is_dp_mst_cap(const char *router, u8 adp, bool remote)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_dp(router, adp))
		return MAX_BIT16;

	if (!remote)
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	else
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_REMOTE_CAP);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & DP_CAP_MST;
}

/*
 * Returns a positive integer if LTTPR capability is not supported on the DP
 * adapter, '0' otherwise.
 * Return a value of 2^32 on any error.
 *
 * @remote: 'true' if the access is for the remote capability.
 */
u64 is_dp_lttpr_sup(const char *router, u8 adp, bool remote)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp(router, adp))
		return MAX_BIT32;

	if (!remote)
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	else
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_REMOTE_CAP);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & DP_CAP_LTTPR;
}

/*
 * Returns a positive integer if bandwidth allocation is supported on the DP
 * adapter, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_in_bw_alloc_sup(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & DP_LOCAL_CAP_IN_BW_ALLOC_SUP;
}

/*
 * Returns a positive integer if DSC capability is not supported on the DP
 * adapter, '0' otherwise.
 * Return a value of 2^32 on any error.
 *
 * @remote: 'true' if the access is for the remote capability.
 */
u64 is_dp_dsc_sup(const char *router, u8 adp, bool remote)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp(router, adp))
		return MAX_BIT32;

	if (!remote)
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_LOCAL_CAP);
	else
		val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_REMOTE_CAP);

	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & DP_CAP_DSC;
}

/*
 * Get the lane count of the DP adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'DP_IN_ADP_LC_XX' definitions in 'adapter.h'.
 */
u16 get_dp_in_lane_count(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & DP_STATUS_LC;
}

/*
 * Get the link rate of the DP adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'DP_ADP_LR_X' definitions in 'adapter.h'.
 */
u16 get_dp_in_link_rate(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & DP_STATUS_LR) >> DP_STATUS_LR_SHIFT;
}

/*
 * Get the allocated bandwidth on the DP adapter.
 * Return a value of 256 on any error.
 */
u16 get_dp_in_alloc_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & DP_STATUS_ABW) >> DP_STATUS_ABW_SHIFT;
}

/*
 * Get the lane count of the DP adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'DP_OUT_ADP_LC_XX' definitions in 'adapter.h'.
 */
u16 get_dp_out_lane_count(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_out(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS_CTRL);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & DP_STATUS_CTRL_LC;
}

/*
 * Get the link rate of the DP adapter.
 * Return a value of 256 on any error.
 *
 * Check the 'DP_ADP_LR_X' definitions in 'adapter.h'.
 */
u16 get_dp_out_link_rate(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_out(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS_CTRL);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & DP_STATUS_CTRL_LR) >> DP_STATUS_CTRL_LR_SHIFT;
}

/*
 * Returns a positive integer if CM has issued handshake, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_out_cm_handshake(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_out(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS_CTRL);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & DP_STATUS_CTRL_CMHS;
}

/*
 * Returns a positive integer if the paired DP IN adapter is a USB4 DP IN adapter,
 * '0' if the adapter is a TBT3 DP IN adapter.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_out_dp_in_usb4(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_out(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS_CTRL);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & DP_STATUS_CTRL_UF;
}

/*
 * Returns a positive integer if DPRX capabilities are read by the DP
 * adapter, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_in_dprx_cap_read_done(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, DP_STATUS_CTRL);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & DP_COMMON_CAP_DPRX_CRD;
}

/*
 * Get the requested bandwidth by DPTX of the DP adapter to the CM.
 * Return a value of 256 on any error.
 */
u16 get_dp_in_req_bw(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT8;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_8);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ADP_DP_CS_8_RBW;
}

/*
 * Returns a positive integer if DPTX bandwidth allocation mode is enabled
 * for the DP adapter, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_in_dptx_bw_alloc_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_8);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_DP_CS_8_DPME;
}

/*
 * Returns a positive integer if DPTX of the DP adapter requests the bandwidth
 * to the CM, '0' if CM has set the 'CM Ack' bit to '1'.
 * Return a value of 2^32 on any error.
 */
u64 is_dp_in_dptx_req(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_dp_in(router, adp))
		return MAX_BIT32;

	val = get_adapter_register_val(router, DP_ADP_CAP_ID, DP_ADP_SEC_ID, adp, ADP_DP_CS_8);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_DP_CS_8_DR;
}
