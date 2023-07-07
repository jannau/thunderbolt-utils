// SPDX-License-Identifier: GPL-2.0
/*
 * TBT/USB4 router configuration space
 *
 * This file provides functions to fetch the router config. space of a
 * TBT/USB4 device (including the ones interoperable with TBT3 systems).
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "helpers.h"

/*
 * Get the topology ID in the below format:
 * "LVL6,LVL5,LVL4,LVL3,LVL2,LVL1", where 'LVL' stands for the adapter
 * number of Lane-0 in the respective level in the topology ID field.
 */
char* get_route_string(u64 top_id)
{
	char *route = malloc(MAX_LEN * sizeof(char));
	char double_const[MAX_LEN];
	s8 i = 6;
	u8 val;

	for (; i >= 0; i--) {
		if (i == 6) {
			val = (top_id & LEVEL_6) >> (8 * i);
			sprintf(double_const, "%02d,", val);
			strcpy(route + ((6 - i) * 3), double_const);
		} else if (i == 5) {
			val = (top_id & LEVEL_5) >> (8 * i);
			sprintf(double_const, "%02d,", val);
			strcpy(route + ((6 - i) * 3), double_const);
		} else if (i == 4) {
			val = (top_id & LEVEL_4) >> (8 * i);
			sprintf(double_const, "%02d,", val);
			strcpy(route + ((6 - i) * 3), double_const);
		} else if (i == 3) {
			val = (top_id & LEVEL_3) >> (8 * i);
			sprintf(double_const, "%02d,", val);
			strcpy(route + ((6 - i) * 3), double_const);
		} else if (i == 2) {
			val = (top_id & LEVEL_2) >> (8 * i);
			sprintf(double_const, "%02d,", val);
			strcpy(route + ((6 - i) * 3), double_const);
		} else if (i == 1) {
			val = (top_id & LEVEL_1) >> (8 * i);
			sprintf(double_const, "%02d,", val);
			strcpy(route + ((6 - i) * 3), double_const);
		} else {
			val = (top_id & LEVEL_0) >> (8 * i);
			sprintf(double_const, "%02d", val);
			strcpy(route + ((6 - i) * 3), double_const);
		}
	}

	return trim_white_space(route);
}

/*
 * Find the upstream adapter (Lane-0) of the provided router.
 * If config. space is inaccessible, return a value of 64.
 */
u8 get_upstream_adp(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_ADAPTERS;

	return (val & ROUTER_CS_1_UPS_ADP) >> ROUTER_CS_1_UPS_ADP_SHIFT;
}

/*
 * Find the max. adapter number in the router.
 * If config. space is inaccessible, return a value of 64.
 */
u8 get_max_adp(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_ADAPTERS;

	return (val & ROUTER_CS_1_MAX_ADP) >> ROUTER_CS_1_MAX_ADP_SHIFT;
}

/*
 * Return the least significant 32 bits of the topology ID.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 get_top_id_low(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val;
}

/*
 * Return the most significant 24 bits of the topology ID.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 get_top_id_high(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_3);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_3_TOP_ID_HIGH;
}

/*
 * Returns the revision no. of the router.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_rev_no(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_1_REV_NO) >> ROUTER_CS_1_REV_NO_SHIFT;
}

/*
 * Returns a positive integer if the router is configured (top. ID is valid),
 * '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_router_configured(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_3);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_3_TOP_ID_VALID;
}

/*
 * Returns the timeout configured (ms) to send the hot event again if CM fails to ack.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_notification_timeout(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ROUTER_CS_4_NOT_TIMEOUT;
}

/*
 * Returns the version of USB4 spec. supported by the CM.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_cmuv(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_4_CMUV) >> ROUTER_CS_4_CMUV_SHIFT;
}

/*
 * Returns the version of the USB4 spec. supported by the router.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_usb4v(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_4_USB4V) >> ROUTER_CS_4_USB4V_SHIFT;
}

/*
 * Returns a positive integer if the wakes are enabled on the provided protocol,
 * '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_wake_enabled(const char *router, u8 protocol)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	if (protocol == PROTOCOL_PCIE)
		return val & ROUTER_CS_5_WOP;
	else if (protocol == PROTOCOL_USB3)
		return val & ROUTER_CS_5_WOU;
	else
		return val & ROUTER_CS_5_WOD;
}

/*
 * Returns a positive integer if the protocol tunneling is turned on, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_tunneling_on(const char *router, u8 protocol)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	if (protocol == PROTOCOL_PCIE)
		return val & ROUTER_CS_5_PTO;
	else if (protocol == PROTOCOL_USB3)
		return val & ROUTER_CS_5_UTO;
	else if (protocol == PROTOCOL_HCI)
		return val & ROUTER_CS_5_IHCO;
	else
		return true;
}

/*
 * Returns a positive integer if CM has enabled the internal HCI, '0'
 * otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_ihci_on(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_5_IHCO;
}

/*
 * Returns a positive integer if tunneling config. is valid, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_tunneling_config_valid(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_5_CV;
}

/*
 * Returns '1' if router is ready to sleep, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_router_sleep_ready(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ROUTER_CS_6_SR;
}

/*
 * Returns a positive integer if the router doesn't support TBT3 compatible behavior,
 * '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_tbt3_not_supported(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ROUTER_CS_6_TNS;
}

/*
 * Returns a positive integer if a wake was caused by the provided protocol,
 * '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_wake_status(const char *router, u8 protocol)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	if (protocol == PROTOCOL_PCIE)
		return val & ROUTER_CS_6_WOPS;
	else if (protocol == PROTOCOL_USB3)
		return val & ROUTER_CS_6_WOUS;
	else
		return val & ROUTER_CS_6_WODS;
}

/*
 * Returns a positive integer if internal HCI is present, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_ihci_present(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_6_IHCI;
}

/*
 * Returns a positive integer if router is ready after CMUV bit gets set to '1',
 * '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_router_ready(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_6_RR;
}

/*
 * Returns a positive integer if router is ready for protocol tunneling
 * provided by the CM, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 is_tunneling_ready(const char *router)
{
	u64 val;

	val = get_router_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_CS_6_CR;
}

/*
 * Returns a positive integer if hot events are disabled on the lane adapters
 * of the TBT3 router, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 is_tbt3_hot_events_disabled_lane(const char *router)
{
	u64 val;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC1_ID,
				      ROUTER_VSEC1_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ROUTER_VSEC1_1_PED_LANE;
}

/*
 * Returns the no. of doublewords in the common region of the VSEC6 capability
 * of the router.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 get_tbt3_com_reg_dwords(const char *router)
{
	u64 val;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID,
				      ROUTER_VSEC6_COM);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_VSEC6_COM_LEN) >> ROUTER_VSEC6_COM_LEN_SHIFT;
}

/*
 * Returns the no. of doublewords in each USB4 port region of the VSEC6 capability
 * of the router.
 * If config. space is inaccessible, return a value of 2^16.
 *
 * Valid only for TBT3 routers.
 */
u32 get_tbt3_usb4_reg_dwords(const char *router)
{
	u64 val;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID,
				      ROUTER_VSEC6_COM);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return (val & ROUTER_VSEC6_COM_USB4_LEN) >> ROUTER_VSEC6_COM_USB4_LEN_SHIFT;
}

/*
 * Returns the no. of USB4 ports present in the router.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 get_tbt3_usb4_ports(const char *router)
{
	u64 val;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID,
				      ROUTER_VSEC6_COM);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ROUTER_VSEC6_COM_PORTS;
}

/*
 * Returns a positive integer if conditions for lane bonding are met, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^16.
 *
 * Valid only for TBT3 routers.
 */
u32 is_tbt3_bonding_en(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT16;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT16;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_PORT_ATTR;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & ROUTER_VSEC6_PORT_ATTR_BE;
}

/*
 * Returns the wake events enabled on the router.
 * If config. space is inaccessible, return a value of 2^16.
 *
 * Valid only for TBT3 routers.
 */
u32 get_tbt3_wake_events_en(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT16;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT16;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LC_SX_CTRL;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & ROUTER_VSEC6_LC_SX_CTRL_EWE;
}

/*
 * Returns the status of the configuration of lanes 0 and 1.
 * If config. space is inaccessible, return a value of 2^32.
 *
 * Valid only for TBT3 routers.
 */
u64 get_tbt3_lanes_configured(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT32;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT32;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LC_SX_CTRL;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & (ROUTER_VSEC6_LC_SX_CTRL_L0C | ROUTER_VSEC6_LC_SX_CTRL_L1C);
}

/*
 * Returns a positive integer if the link connected to the port is operating in
 * TBT3 compatible mode, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 *
 * Valid only for TBT3 routers.
 */
u64 is_tbt3_compatible_mode(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT32;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT32;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LINK_ATTR;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_VSEC6_LINK_ATTR_TCM;
}

/*
 * Returns a positive integer if CLx is supported on the lane, '0' otherwise.
 * If config. space is inaccessible, return a value of 2^32.
 *
 * Valid only for TBT3 routers.
 */
u64 is_tbt3_clx_supported(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT32;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT32;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LINK_ATTR;

	val = get_router_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ROUTER_VSEC6_LINK_ATTR_CPS;
}

/*
 * Returns the port number of the provided lane adapter number.
 * For e.g., lane adapters 2 and 4 will correspond to port numbers
 * 0 and 1 respectively.
 *
 * Caller needs to ensure that the lane adapter number is valid.
 */
u8 get_usb4_port_num(u8 lane_adp)
{
	return (lane_adp - 1) / 2;
}
