// SPDX-License-Identifier: GPL-2.0
/*
 * User-space utility for the thunderbolt/USB4 subsystem
 *
 * This file adds the verbose functionalities (lspci-like) of the TBT/USB4
 * subsystem.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "helpers.h"

#define VERBOSE_L1_SPACES		21
#define VERBOSE_L2_SPACES		29
#define VERBOSE_L3_SPACES		37

/* Dump the vendor/device name of the router */
static void dump_name(const char *router)
{
	char v_path[MAX_LEN], d_path[MAX_LEN], vcheck[MAX_LEN], dcheck[MAX_LEN];
	char *vendor, *device;

	snprintf(vcheck, sizeof(vcheck), "%s%s/vendor_name", tbt_sysfs_path, router);
	snprintf(dcheck, sizeof(dcheck), "%s%s/device_name", tbt_sysfs_path, router);
	if (is_link_nabs(vcheck) || is_link_nabs(dcheck))
		exit(1);

	snprintf(v_path, sizeof(v_path), "cat %s%s/vendor_name", tbt_sysfs_path, router);
	vendor = do_bash_cmd(v_path);

	snprintf(d_path, sizeof(d_path), "cat %s%s/device_name", tbt_sysfs_path, router);
	device = do_bash_cmd(d_path);

	printf("%s %s ", vendor, device);

	free(vendor);
	free(device);
}

static void dump_spaces(u64 spaces)
{
	while (spaces--)
		printf(" ");
}

/*
 * Returns the router state respectively among one of the following:
 * 1. Uninitialized plugged
 * 2. Enumerated
 *
 * NOTE: 'Uninitialized unplugged' state can be ignored since router exists
 * and thus its upstream port is connected.
 */
static char* get_router_state(const char *router)
{
	u64 configured;

	configured = is_router_configured(router);
	if (configured == MAX_BIT32)
		return "<Not accessible>";
	else if (configured)
		return "Enumerated";
	else
		return "Uninitialized plugged";
}

/* Dumps the thunderbolt compatibility (TBT3 as of now) of the router */
static void dump_tbt_compatibility(const char *router)
{
	u16 tbt3_not_sup;

	dump_spaces(VERBOSE_L2_SPACES);

	printf("Thunderbolt: ");

	tbt3_not_sup = is_tbt3_not_supported(router);
	if (tbt3_not_sup == MAX_BIT8)
		printf("<Not accessible>\n");
	else
		tbt3_not_sup ? printf("TBT3-\n") : printf("TBT3+\n");
}

/*
 * Returns the router connected to the upstream port of the provided router.
 * In case of a host router, return the router itself.
 */
static char* get_upstream_router(char *router)
{
	char *ups_router = malloc(MAX_LEN * sizeof(char));
	char path[MAX_LEN];
	u64 pos1, pos2;
	char *output;

	if (is_host_router(router)) {
		strcpy(ups_router, router);
		return ups_router;
	}

	snprintf(path, sizeof(path), "readlink %s%s", tbt_sysfs_path, router);
	output = do_bash_cmd(path);

	pos2 = strrchr(output, '/') - output;
	pos1 = pos2 - 1;

	while (pos1--) {
		if (output[pos1] == '/')
			break;
	}

	memset(ups_router, '\0', MAX_LEN * sizeof(char));
	strncpy(ups_router, output + pos1 + 1, pos2 - pos1 - 1);

	free(output);

	return ups_router;
}

static u64 map_lvl_to_bitmask(u8 depth)
{
	if (depth == 0)
		return LEVEL_0;
	else if (depth == 1)
		return LEVEL_1;
	else if (depth == 2)
		return LEVEL_2;
	else if (depth == 3)
		return LEVEL_3;
	else if (depth == 4)
		return LEVEL_4;
	else if (depth == 5)
		return LEVEL_5;
	else if (depth == 6)
		return LEVEL_6;

	return COMPLEMENT_BIT64;
}

/*
 * Returns the downstream port (Lane-0) of the upstream router to which the
 * provided router is connected.
 * Return a value of '0' in case of host routers.
 */
static u8 get_ups_down_port(const char *router)
{
	u64 topid_low, topid_high, top_id;
	u64 level_bitmask;
	s8 depth;

	depth = depth_of_router(router) - 1;

	if (is_host_router(router) || (depth < 0))
		return 0;

	topid_low = get_top_id_low(router);
	if (topid_low == MAX_BIT32)
		return MAX_ADAPTERS;

	topid_high = get_top_id_high(router);
	if (topid_high == MAX_BIT32)
		return MAX_ADAPTERS;

	top_id = topid_high << 23 | topid_low;

	level_bitmask = map_lvl_to_bitmask(depth);
	return (top_id & level_bitmask) >> (8 * depth);
}


/* Dump the power-states (sleep as of now) support for the router */
static void dump_power_states_compatibility(char *router)
{
	char *ups_router;
	u64 lanes_conf;
	u8 down_port;
	u16 usb4v;
	u8 majv;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("PWR-support: ");

	if (is_host_router(router)) {
		printf("Sleep+\n");
		return;
	}

	ups_router = get_upstream_router(router);

	down_port = get_ups_down_port(router);
	if (down_port == MAX_ADAPTERS)
		goto free;

	usb4v = get_usb4v(router);
	if (usb4v == MAX_BIT8)
		goto free;

	majv = (usb4v & USB4V_MAJOR_VER) >> USB4V_MAJOR_VER_SHIFT;
	if (!majv) {
		lanes_conf = get_tbt3_lanes_configured(ups_router,
						       get_usb4_port_num(down_port));
		if (lanes_conf == MAX_BIT32)
			printf("<Not accessible>\n");
		else if ((lanes_conf & ROUTER_VSEC6_LC_SX_CTRL_L0C) &&
			 (lanes_conf & ROUTER_VSEC6_LC_SX_CTRL_L1C))
			printf("Sleep+\n");
		else
			printf("Sleep-\n");
	} else {
		lanes_conf = is_usb4_port_configured(router, down_port);
		if (lanes_conf == MAX_BIT8)
			printf("<Not accessible>\n");
		else if (lanes_conf)
			printf("Sleep+\n");
		else
			printf("Sleep-\n");
	}

free:
	free(ups_router);
}

/*
 * Dump the IHCI status.
 * Caller needs to ensure the parameter passed is a device router.
 */
static void dump_ihci_status(const char *router)
{
	u64 imp, on, cv;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("Internal HCI: ");

	imp = is_ihci_present(router);
	if (imp == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	}

	cv = is_tunneling_config_valid(router);
	if (cv == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	}

	on = is_ihci_on(router);
	if (on == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	}

	if (imp) {
		if (cv && on)
			printf("Pres+ En+\n");
		else
			printf("Pres+ En-\n");
	} else
		printf("Pres-\n");
}

/*
 * Dump the PCIe tunneling status of the router.
 * Caller needs to ensure the parameter passed is a device router.
 */
static void dump_pcie_tunneling_status(const char *router)
{
	u64 pcie, cv, cr;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("PCIe: ");

	cv = is_tunneling_config_valid(router);
	cr = is_tunneling_ready(router);

	if (cv == MAX_BIT32 || cr == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	}

	pcie = is_tunneling_on(router, PROTOCOL_PCIE);
	if (pcie == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	} else if (pcie) {
		if (cv && cr)
			printf("On\n");
		else
			printf("Off\n");
	} else
		printf("Off\n");
}

/*
 * Dump the USB3 tunneling status of the router.
 * Caller needs to ensure the parameter passed is a device router.
 */
static void dump_usb3_tunneling_status(const char *router)
{
	u64 usb3, cv, cr;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("USB3: ");

	cv = is_tunneling_config_valid(router);
	cr = is_tunneling_ready(router);

	if (cv == MAX_BIT32 || cr == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	}

	usb3 = is_tunneling_on(router, PROTOCOL_USB3);
	if (usb3 == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	} else if (usb3) {
		if (cv && cr)
			printf("On\n");
		else
			printf("Off\n");
	} else
		printf("Off\n");
}

/* Dumps the notification timeout configured in the router */
static void dump_not_timeout(const char *router)
{
	u16 timeout;

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Notification timeout: ");

	timeout = get_notification_timeout(router);
	if (timeout == MAX_BIT8)
		printf("<Not accessible>\n");
	else
		printf("%ums\n", timeout);
}

/*
 * Dumps the wakes enabled on various hot events in the router.
 * Only applicable for TBT3 routers.
 */
static void dump_tbt3_wake_hot_events(u32 wakes)
{
	printf("Hot plugs: ");

	if (wakes & TBT3_HOT_PLUG_ROUTER)
		printf("Router+ ");
	else
		printf("Router- ");

	if (wakes & TBT3_HOT_PLUG_DP)
		printf("DP+ ");
	else
		printf("DP- ");

	if (wakes & TBT3_HOT_PLUG_USB)
		printf("USB+\n");
	else
		printf("USB-\n");

	dump_spaces(VERBOSE_L3_SPACES);
	printf("Hot unplugs: ");

	if (wakes & TBT3_HOT_UNPLUG_ROUTER)
		printf("Router+ ");
	else
		printf("Router- ");

	if (wakes & TBT3_HOT_UNPLUG_DP)
		printf("DP+ ");
	else
		printf("DP- ");

	if (wakes & TBT3_HOT_UNPLUG_USB)
		printf("USB+\n");
	else
		printf("USB-\n");
}

/*
 * Dumps the wakes enabled on various event indications.
 * Only applicable for TBT3 routers.
 */
static void dump_tbt3_gen_wakes(u32 wakes)
{
	dump_spaces(VERBOSE_L3_SPACES);
	printf("Wake indication: ");

	if (wakes & TBT3_USB4)
		printf("USB4+ ");
	else
		printf("USB4- ");

	if (wakes & TBT3_PCIE)
		printf("PCIe+\n");
	else
		printf("PCIe-\n");
}

/*
 * Dumps the wakes enabled on various event indications.
 * Caller needs to ensure the parameter passed is a USB4 device router.
 */
static void dump_usb4_gen_wakes(const char *router)
{
	u16 wakes;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("Wake indication: ");

	wakes = is_wake_enabled(router, PROTOCOL_PCIE);
	if (wakes == MAX_BIT8) {
		printf("<Not accessible>\n");
		return;
	} else if (wakes)
		printf("PCIe+ ");
	else
		printf("PCIe- ");

	wakes = is_wake_enabled(router, PROTOCOL_USB3);
	if (wakes)
		printf("USB3+ ");
	else
		printf("USB3- ");

	wakes = is_wake_enabled(router, PROTOCOL_DP);
	if (wakes)
		printf("DP+\n");
	else
		printf("DP-\n");
}

/*
 * Dumps the USB4 port-specific wakes enabled in the router.
 * Caller needs to ensure the parameter passed is a USB4 router.
 */
static void dump_usb4_port_wakes(const char *router, u8 adp)
{
	u64 wakes;

	wakes = get_usb4_wakes_en(router, adp);
	if (wakes == MAX_BIT32) {
		printf("<Not accessible>\n");
		return;
	}

	printf("Hot events: ");

	if (wakes & PORT_CS_19_EWOC)
		printf("Connect+ ");
	else
		printf("Connect- ");

	if (wakes & PORT_CS_19_EWOD)
		printf("Disconnect+\n");
	else
		printf("Disconnect-\n");

	dump_spaces(VERBOSE_L3_SPACES);
	printf("Wake indication: ");

	if (wakes & PORT_CS_19_EWOU4)
		printf("USB4+\n");
	else
		printf("USB4-\n");
}

/* Dumps the wakes enabled in the router */
static void dump_wakes(const char *router)
{
	u8 max_adp, port;
	u32 wakes;
	u16 usb4v;
	u8 i = 0;
	u8 majv;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("<Not accessible>\n");

		return;
	}

	usb4v = get_usb4v(router);
	if (usb4v == MAX_BIT8) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("<Not accessible>\n");

		return;
	}

	majv = (usb4v & USB4V_MAJOR_VER) >> USB4V_MAJOR_VER_SHIFT;
	if (!majv) {
		for (; i <= max_adp; i++) {
			if (!is_adp_lane_0(router, i))
				continue;

			port = get_usb4_port_num(i);

			dump_spaces(VERBOSE_L2_SPACES);
			printf("Port %u: ", i);

			wakes = get_tbt3_wake_events_en(router, port);
			if (wakes == MAX_BIT16) {
				printf("<Not accessible>\n");
				continue;
			}

			dump_tbt3_wake_hot_events(wakes);
			dump_tbt3_gen_wakes(wakes);
		}
	} else {
		i = 0;

		if (!is_host_router(router))
			dump_usb4_gen_wakes(router);

		for (; i <= max_adp; i++) {
			if (!is_adp_lane_0(router, i))
				continue;

			dump_spaces(VERBOSE_L2_SPACES);
			printf("Port %u: ", i);

			dump_usb4_port_wakes(router, i);
		}
	}
}

/*
 * Dumps the wake status (if any) from various protocols (PCIe/USB3/DP).
 * Applicable for both USB4 and TBT3 routers.
 */
static void dump_gen_wake_status(const char *router)
{
	u16 sts;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("Wake indication: ");

	sts = get_wake_status(router, PROTOCOL_PCIE);
	if (sts == MAX_BIT8) {
		printf("<Not accessible>\n");
		return;
	} else if (sts)
		printf("PCIe+ ");
	else
		printf("PCIe- ");

	sts = get_wake_status(router, PROTOCOL_USB3);
	if (sts)
		printf("USB3+ ");
	else
		printf("USB3- ");

	sts = get_wake_status(router, PROTOCOL_DP);
	if (sts)
		printf("DP+\n");
	else
		printf("DP-\n");
}

/*
 * Dumps the wake status (if any) from connects/disconnects, or
 * from a USB4 wake indication.
 * Only applicable for USB4 routers.
 */
static void dump_usb4_port_wake_status(u64 status)
{
	printf("Hot events: ");

	if (status & PORT_CS_18_WOCS)
		printf("Connect+ ");
	else
		printf("Connect- ");

	if (status & PORT_CS_18_WODS)
		printf("Disconnect+\n");
	else
		printf("Disconnect-\n");

	dump_spaces(VERBOSE_L3_SPACES);
	printf("Wake indication: ");

	if (status & PORT_CS_18_WOU4S)
		printf("USB4+\n");
	else
		printf("USB4-\n");
}

/* Dumps the wake status from various events in the router */
static void dump_wake_status(const char *router)
{
	u8 max_adp;
	u64 status;
	u16 usb4v;
	u8 i = 0;
	u8 majv;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("<Not accessible>\n");

		return;
	}

	if (!is_host_router(router))
		dump_gen_wake_status(router);

	usb4v = get_usb4v(router);
	if (usb4v == MAX_BIT8) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("Ports: <Not accessible>\n");

		return;
	}

	majv = (usb4v & USB4V_MAJOR_VER) >> USB4V_MAJOR_VER_SHIFT;
	if (majv) {
		for (; i <= max_adp; i++) {
			if (!is_adp_lane_0(router, i))
				continue;

			dump_spaces(VERBOSE_L2_SPACES);
			printf("Port %u: ", i);

			status = get_usb4_wake_status(router, i);
			if (status == MAX_BIT32) {
				printf("<Not accessible>\n");
				continue;
			}

			dump_usb4_port_wake_status(status);
		}
        }
}

/* Returns the total no. of USB3 adapters */
static u8 get_usb3_adps_num(const char *router)
{
	u8 i = 1, count = 0;
	u8 max_adp;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		return MAX_ADAPTERS;

	for (; i <= max_adp; i++) {
		if (is_adp_usb3(router, i))
			count++;
	}

	return count;
}


/* Returns the total no. of PCIe adapters */
static u8 get_pcie_adps_num(const char *router)
{
	u8 i = 1, count = 0;
	u8 max_adp;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		return MAX_ADAPTERS;

	for (; i <= max_adp; i++) {
		if (is_adp_pcie(router, i))
			count++;
	}

	return count;
}

/* Returns the total no. of DP adapters */
static u8 get_dp_adps_num(const char *router)
{
	u8 i = 1, count = 0;
	u8 max_adp;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		return MAX_ADAPTERS;

	for (; i <= max_adp; i++) {
		if (is_adp_dp(router, i))
			count++;
	}

	return count;
}

/* Dumps the no. of protocol adapters in the router */
static void dump_adapters_num(const char *router)
{
	u8 usb3, pcie, dp;

	usb3 = get_usb3_adps_num(router);
	pcie = get_pcie_adps_num(router);
	dp = get_dp_adps_num(router);

	dump_spaces(VERBOSE_L2_SPACES);

	if (usb3 == MAX_ADAPTERS)
		printf("USB3: <Not accessible>\n");
	else
		printf("USB3:%u\n", usb3);

	dump_spaces(VERBOSE_L2_SPACES);

	if (pcie == MAX_ADAPTERS)
		printf("PCIe: <Not accessible>\n");
	else
		printf("PCIe:%u\n", pcie);

	dump_spaces(VERBOSE_L2_SPACES);

	if (dp == MAX_ADAPTERS)
		printf("DP: <Not accessible>\n");
	else
		printf("DP:%u\n", dp);
}

static u32 usb3_bw_to_mbps(u32 bw, u16 scale)
{
	double mbps;

	mbps = ((double)((bw * 512) << scale) * 8000) / (1000 * 1000);
	return round(mbps);
}

static void dump_usb3_pls(u16 pls)
{
	printf("Port link state: ");

	if (pls == MAX_BIT8)
		printf("<Not accessible>\n");
	else if (pls == USB3_PLS_U0)
		printf("U0\n");
	else if (pls == USB3_PLS_U2)
		printf("U2\n");
	else if (pls == USB3_PLS_U3)
		printf("U3\n");
	else if (pls == USB3_PLS_DISABLED)
		printf("Disabled\n");
	else if (pls == USB3_PLS_RX_DETECT)
		printf("RX.Detect\n");
	else if (pls == USB3_PLS_INACTIVE)
		printf("Inactive\n");
	else if (pls == USB3_PLS_POLLING)
		printf("Polling\n");
	else if (pls == USB3_PLS_RECOVERY)
		printf("Recovery\n");
	else if (pls == USB3_PLS_HOT_RESET)
		printf("Hot.Reset\n");
	else if (USB3_PLS_RESUME)
		printf("Resume\n");
}

/*
 * Dumps the consumed and allocated upstream/downstream bandwidths of the
 * provided USB3 adapters (only applicable for host routers), their
 * actual and max. supported link rates, and their port link states.
 *
 * @active: Array containing the active USB3 adapter numbers in the router.
 */
static void dump_usb3_bws_lr_pls(const char *router, u8 active[])
{
	u16 scale, ulv, alr, mlr, pls;
	u32 cub, cdb, aub, adb;
	u8 i = 0;

	for (; i < MAX_ADAPTERS; i++) {
		char num[MAX_LEN];
		u64 spaces;

		if (!active[i])
			return;

		dump_spaces(VERBOSE_L3_SPACES);
		printf("%u: ", active[i]);

		snprintf(num, sizeof(num), "%u: ", active[i]);
		spaces = strlen(num);

		if (is_host_router(router)) {
			scale = get_usb3_scale(router, active[i]);
			cub = get_usb3_consumed_up_bw(router, active[i]);
			cdb = get_usb3_consumed_down_bw(router, active[i]);
			aub = get_usb3_allocated_up_bw(router, active[i]);
			adb = get_usb3_allocated_down_bw(router, active[i]);

			if (cub == MAX_BIT16 || scale == MAX_BIT8)
				printf("Consumed UP b/w: <Not accessible>\n");
			else
				printf("Consumed UP b/w: %u\n",
					usb3_bw_to_mbps(cub, scale));

			dump_spaces(VERBOSE_L3_SPACES + spaces);

			if (cdb == MAX_BIT16 || scale == MAX_BIT8)
				printf("Consumed DOWN b/w: <Not accessible>\n");
			else
				printf("Consumed DOWN b/w: %u\n",
					usb3_bw_to_mbps(cdb, scale));

			dump_spaces(VERBOSE_L3_SPACES + spaces);

			if (aub == MAX_BIT16 || scale == MAX_BIT8)
				printf("Allocated UP b/w: <Not accessible>\n");
			else
				printf("Allocated UP b/w: %u\n",
					usb3_bw_to_mbps(aub, scale));

			dump_spaces(VERBOSE_L3_SPACES + spaces);

			if (adb == MAX_BIT16 || scale == MAX_BIT8)
				printf("Allocated DOWN b/w: <Not accessible>\n");
			else
				printf("Allocated DOWN b/w: %u\n",
					usb3_bw_to_mbps(adb, scale));

			dump_spaces(VERBOSE_L3_SPACES + spaces);
		}

		ulv = is_usb3_link_valid(router, active[i]);
		alr = get_usb3_actual_lr(router, active[i]);
		mlr = get_usb3_max_sup_lr(router, active[i]);

		if (ulv == MAX_BIT8 || alr == MAX_BIT8)
			printf("Actual link rate: <Not accessible>\n");
		else if (ulv && alr == USB3_LR_GEN2_SL)
			printf("Actual link rate: 10Gbps\n");
		else if (ulv && alr == USB3_LR_GEN2_DL)
			printf("Actual link rate: 20Gbps\n");

		if (ulv == MAX_BIT8 || mlr == MAX_BIT8)
			printf("Max. supported link rate: <Not accessible>\n");
		else if (mlr == USB3_LR_GEN2_SL)
			printf("Max. supported link rate: 10Gbps\n");
		else if (mlr == USB3_LR_GEN2_DL)
			printf("Max. supported link rate: 20Gbps\n");

		pls = get_usb3_port_link_state(router, active[i]);

		dump_spaces(VERBOSE_L3_SPACES + spaces);
		dump_usb3_pls(pls);
	}
}

/* Dumps the verbose output for downstream USB3 adapters in the router */
static void dump_down_usb3_adapters(const char *router)
{
	u8 active[MAX_ADAPTERS];
	bool found = false;
	int i, last_num;
	u8 num, j;
	u64 en;

	num = get_usb3_adps_num(router);
	if (num == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("Downstream USB3: <Not accessible>\n");

		return;
	}

	for (i = MAX_ADAPTERS - 1; i >=0; i--) {
		if (is_adp_down_usb3(router ,i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));
	j = 0;

	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (!is_adp_down_usb3(router, i))
			continue;

		if (!found) {
			dump_spaces(VERBOSE_L2_SPACES);
			printf("Downstream USB3: ");

			found = true;
                }

		printf("%u", i);

		en = is_usb3_adp_en(router, i);
		if (en == MAX_BIT32)
			printf("<Not accessible>");
		else if (en) {
			printf("+");
			active[j++] = i;
		} else
			printf("-");

		if (i == last_num)
			printf("\n");
		else
			printf(" ");
	}

	dump_usb3_bws_lr_pls(router, active);
}

/* Dumps the verbose output for upstream USB3 adapters in the router */
static void dump_up_usb3_adapters(const char *router)
{
	u8 active[MAX_ADAPTERS];
	bool found = false;
	int i, last_num;
	u8 num, j;
	u64 en;

	num = get_usb3_adps_num(router);
	if (num == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("Upstream USB3: <Not accessible>\n");

		return;
	}

	for (i = MAX_ADAPTERS - 1; i >=0; i--) {
		if (is_adp_up_usb3(router ,i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));
	j = 0;

	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (!is_adp_up_usb3(router, i))
			continue;

		if (!found) {
			dump_spaces(VERBOSE_L2_SPACES);
			printf("Upstream USB3: ");

			found = true;
		}

		printf("%u", i);

		en = is_usb3_adp_en(router, i);
		if (en == MAX_BIT32)
			printf("<Not accessible>");
		else if (en) {
			printf("+");
			active[j++] = i;
		} else
			printf("-");

		if (i == last_num)
			printf("\n");
		else
			printf(" ");
	}

	dump_usb3_bws_lr_pls(router, active);
}

static void dump_pcie_ltssm(u16 ltssm)
{
	printf("LTSSM: ");

	if (ltssm == MAX_BIT8)
		printf("<Not accessible>\n");
	else if (ltssm == PCIE_LTSSM_DETECT)
		printf("Detect\n");
	else if (ltssm == PCIE_LTSSM_POLLING)
		printf("Polling\n");
	else if (ltssm == PCIE_LTSSM_CONFIGURATION)
		printf("Configuration\n");
	else if (ltssm == PCIE_LTSSM_CONFIGURATION_IDLE)
		printf("Configuration.Idle\n");
	else if (ltssm == PCIE_LTSSM_RECOVERY)
		printf("Recovery\n");
	else if (ltssm == PCIE_LTSSM_RECOVERY_IDLE)
		printf("Recovery.Idle\n");
	else if (ltssm == PCIE_LTSSM_L0)
		printf("L0\n");
	else if (ltssm == PCIE_LTSSM_L1)
		printf("L1\n");
	else if (ltssm == PCIE_LTSSM_L2)
		printf("L2\n");
	else if (ltssm == PCIE_LTSSM_DISABLED)
		printf("Disabled\n");
	else if (ltssm == PCIE_LTSSM_HOT_RESET)
		printf("Hot reset\n");
}

/*
 * Dumps various PCIe attributes of the PCIe adapters in the router including:
 * 1. PHY state
 * 2. TX EI
 * 3. RX EI
 * 4. PCIe switch port state
 * 5. LTSSM
 *
 * @active: Contains the active PCIe adapters in the router.
 */
static void dump_pcie_attributes(const char *router, u8 active[])
{
	u64 phy, tx_ei, rx_ei, wr;
	u16 ltssm;
	u8 i = 0;

	for (; i < MAX_ADAPTERS; i++) {
		char num[MAX_LEN];
		u64 spaces;

		if (!active[i])
			return;

		dump_spaces(VERBOSE_L3_SPACES);
		printf("%u: ", active[i]);

		snprintf(num, sizeof(num), "%u: ", active[i]);
		spaces = strlen(num);

		phy = is_pcie_link_up(router, active[i]);
		tx_ei = is_pcie_tx_ei(router, active[i]);
		rx_ei = is_pcie_rx_ei(router, active[i]);
		wr = is_pcie_switch_warm_reset(router, active[i]);
		ltssm = get_pcie_ltssm(router, active[i]);

		if (phy == MAX_BIT32)
			printf("PHY: <Not accessible>\n");
		else if (phy)
			printf("PHY: Active\n");
		else
			printf("PHY: Inactive\n");

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (tx_ei == MAX_BIT32)
			printf("TX: Electrical idle (<Not accessible>)\n");
		else if (tx_ei)
			printf("TX: Electrical idle (yes)\n");
		else
			printf("TX: Electrical idle (no)\n");

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (rx_ei == MAX_BIT32)
			printf("RX: Electrical idle (<Not accessible>)\n");
		else if (rx_ei)
			printf("RX: Electrical idle (yes)\n");
		else
			printf("RX: Electrical idle (no)\n");

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (wr == MAX_BIT32)
			printf("PCIe switch port: Warm reset (<Not accessible>)\n");
		else if (wr)
			printf("PCIe switch port: Warm reset (yes)\n");
		else
			printf("PCIe switch port: Warm reset (no)\n");

		dump_spaces(VERBOSE_L3_SPACES + spaces);
		dump_pcie_ltssm(ltssm);
	}
}

/* Dumps the verbose output of the downstream PCIe adapters in the router */
static void dump_down_pcie_adapters(const char *router)
{
	u8 active[MAX_ADAPTERS];
	bool found = false;
	int i, last_num;
	u8 num, j;
	u64 en;

	num = get_pcie_adps_num(router);
	if (num == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("Downstream PCIe: <Not accessible>\n");

		return;
	}

	for (i = MAX_ADAPTERS - 1; i >=0; i--) {
		if (is_adp_down_pcie(router, i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));;
	j = 0;

	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (!is_adp_down_pcie(router, i))
			continue;

		if (!found) {
			dump_spaces(VERBOSE_L2_SPACES);
			printf("Downstream PCIe: ");

			found = true;
		}

		printf("%u", i);

		en = is_pcie_adp_enabled(router, i);
		if (en == MAX_BIT32)
			printf("<Not accessible>");
		else if (en) {
			printf("+");
			active[j++] = i;
		} else
			printf("-");

		if (i == last_num)
			printf("\n");
		else
			printf(" ");
	}

	dump_pcie_attributes(router, active);
}

/* Dumps the verbose output of the upstream PCIe adapters in the router */
static void dump_up_pcie_adapters(const char *router)
{
	u8 active[MAX_ADAPTERS];
	bool found = false;
	int i, last_num;
	u8 num, j;
	u64 en;

	num = get_pcie_adps_num(router);
	if (num == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("Upstream PCIe: <Not accessible>\n");

		return;
	}

	for (i = MAX_ADAPTERS - 1; i >=0; i--) {
		if (is_adp_up_pcie(router, i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));;
	j = 0;

	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (!is_adp_up_pcie(router, i))
			continue;

		if (!found) {
			dump_spaces(VERBOSE_L2_SPACES);
			printf("Upstream PCIe: ");

			found = true;
		}

		printf("%u", i);

		en = is_pcie_adp_enabled(router, i);
		if (en == MAX_BIT32)
			printf("<Not accessible>");
		else if (en) {
			printf("+");
			active[j++] = i;
		} else
			printf("-");

		if (i == last_num)
			printf("\n");
		else
			printf(" ");
	}

	dump_pcie_attributes(router, active);
}

/*
 * Returns a positive integer if the DP adapter is enabled, '0' otherwise.
 * Return a value of 2^32 on any error.
 */
static u64 is_dp_adp_enabled(const char *router, u8 adp)
{
	u64 aux_en, vid_en;

	aux_en = is_dp_aux_en(router, adp);
	if (aux_en == MAX_BIT32)
		return MAX_BIT32;

	vid_en = is_dp_vid_en(router, adp);
	return aux_en || vid_en;
}

static void dump_lr(u32 lr)
{
	if (lr == MAX_BIT16)
		printf("<Not accessible>\n");
	else if (lr == DP_ADP_LR_RBR)
		printf("RBR(1.62GHz)\n");
	else if (lr == DP_ADP_LR_HBR)
		printf("HBR(2.7GHz)\n");
	else if (lr == DP_ADP_LR_HBR2)
		printf("HBR2(5.4GHz)\n");
	else if (lr == DP_ADP_LR_HBR3)
		printf("HBR3(8.1GHz)\n");
}

static void dump_lc(u32 lc)
{
	if (lc == MAX_BIT16)
		printf("<Not accessible>\n");
	else if (lc == DP_ADP_MAX_LC_X1)
		printf("x1\n");
	else if (lc == DP_ADP_MAX_LC_X2)
		printf("x2\n");
	else if (lc == DP_ADP_MAX_LC_X4)
		printf("x4\n");
}

static double dp_ebw_to_bw(u16 ebw, u16 gr)
{
	if (gr == DP_IN_BW_GR_QUARTER)
		return 0.25 * ebw;
	else if (gr == DP_IN_BW_GR_HALF)
		return 0.5 * ebw;
	else if (gr == DP_IN_BW_GR_FULL)
		return ebw;

	return -1;
}

/*
 * Dumps various DP attributes of the DP adapters in the router provided,
 * including:
 * 1. MST and DSC capability
 * 2. HPD status
 * 3. Max. link rate and lane count
 * 4. LTTPR capability
 * 5. Dynamic b/w allocation parameters (for DP IN adapters only)
 *
 * @active: Contains the active DP adapters of the router.
 */
static void dump_dp_attributes(const char *router, u8 active[])
{
	u16 hpd, ebw, nrd_mlc, nrd_mlr, mlr, mlc, gr, abw, rbw;
	u64 dsc, lttpr, bwsup, cmms, dpme, dr;
	u8 i = 0;
	u32 mst;

	for (; i < MAX_ADAPTERS; i++) {
		char num[MAX_LEN];
		u64 spaces;

		if (!active[i])
			return;

		dump_spaces(VERBOSE_L3_SPACES);
		printf("%u: ", active[i]);

		snprintf(num, sizeof(num), "%u: ", active[i]);
		spaces = strlen(num);

		mst = is_dp_mst_cap(router, active[i], false);
		dsc = is_dp_dsc_sup(router, active[i], false);
		hpd = get_dp_hpd_status(router, active[i]);
		mlr = get_dp_max_link_rate(router, active[i], false);
		mlc = get_dp_max_lane_count(router, active[i], false);
		lttpr = is_dp_lttpr_sup(router, active[i], false);

		if (mst == MAX_BIT16)
			printf("MST: <Not accessible> ");
		else if (mst)
			printf("MST+ ");
		else
			printf("MST- ");

		if (dsc == MAX_BIT32)
			printf("DSC: <Not accessible>\n");
		else if (dsc)
			printf("DSC-\n");
		else
			printf("DSC+\n");

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (hpd == MAX_BIT8)
			printf("HPD: <Not accessible>\n");
		else if (hpd)
			printf("HPD+\n");
		else
			printf("HPD-\n");

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		printf("Max. link rate: ");
		dump_lr(mlr);

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		printf("Max. lane count: ");
		dump_lc(mlc);

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (lttpr == MAX_BIT32)
			printf("<Not accessible>\n");
		else if (lttpr)
			printf("LTTPR-\n");
		else
			printf("LTTPR+\n");

		if (!is_adp_dp_in(router, active[i]))
			continue;

		bwsup = is_dp_in_bw_alloc_sup(router, active[i]);
		cmms = is_dp_in_cm_bw_alloc_support(router, active[i]);
		dpme = is_dp_in_dptx_bw_alloc_en(router, active[i]);
		ebw = get_dp_in_estimated_bw(router, active[i]);
		nrd_mlc = get_dp_in_nrd_max_lc(router, active[i]);
		nrd_mlr = get_dp_in_nrd_max_lr(router, active[i]);
		gr = get_dp_in_granularity(router, active[i]);
		abw = get_dp_in_alloc_bw(router, active[i]);
		rbw = get_dp_in_req_bw(router, active[i]);
		dr = is_dp_in_dptx_req(router, active[i]);

		dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (bwsup == MAX_BIT32 || cmms == MAX_BIT32 ||
		    dpme == MAX_BIT32)
			printf("Bandwidth-alloc: <Not accessible>\n");
		else if (bwsup && dpme) {
			u64 bw_spaces;

			printf("Bandwidth-alloc: Pres+ ");

			if (cmms) {
				printf("En+\n");

				snprintf(num, sizeof(num), "Bandwidth-alloc: ");
				bw_spaces = strlen(num);

				dump_spaces(VERBOSE_L3_SPACES + spaces + bw_spaces);

				if (ebw == MAX_BIT8 || gr == MAX_BIT8)
					printf("Estimated b/w: <Not accessible>\n");
				else
					printf("Estimated b/w: %lfMbps\n",
						dp_ebw_to_bw(ebw, gr));

				dump_spaces(VERBOSE_L3_SPACES + spaces + bw_spaces);

				printf("Non reduced max. link rate: ");
				dump_lr(nrd_mlr);

				printf("Non reduced max. lane count: ");
				dump_lc(nrd_mlc);

				dump_spaces(VERBOSE_L3_SPACES + spaces + bw_spaces);

				if (abw == MAX_BIT8 || gr == MAX_BIT8)
					printf("Allocated b/w: <Not accessible>\n");
				else
					printf("Allocated b/w: %lfMbps\n",
						dp_ebw_to_bw(ebw, gr));

				dump_spaces(VERBOSE_L3_SPACES + spaces + bw_spaces);

				if (rbw == MAX_BIT8 || gr == MAX_BIT8 ||
				    dr == MAX_BIT32)
					printf("Requested b/w: <Not accessible>\n");
				else if (!dr)
					printf("Requested b/w: %lfMbps (served)\n",
						dp_ebw_to_bw(ebw, gr));
				else
					printf("Requested b/w: %lfMbps (unserved)\n",
						dp_ebw_to_bw(ebw, gr));
			} else
				printf("En-\n");
		} else
			printf("Bandwidth-alloc: Pres-\n");
	}
}

/* Dumps the verbose output for DP OUT adapters */
static void dump_dp_out_adapters(const char *router)
{
	u8 active[MAX_ADAPTERS];
	bool found = false;
	int i, last_num;
	u8 num, j;
	u64 en;

	num = get_dp_adps_num(router);
	if (num == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("DP OUT: <Not accessible>\n");

		return;
	}

	for (i = MAX_ADAPTERS - 1; i >=0; i--) {
		if (is_adp_dp_out(router, i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));;
	j = 0;

	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (!is_adp_dp_out(router, i))
			continue;

		if (!found) {
			dump_spaces(VERBOSE_L2_SPACES);
			printf("DP OUT: ");

			found = true;
		}

		printf("%u", i);

		en = is_dp_adp_enabled(router, i);
		if (en == MAX_BIT32)
			printf("<Not accessible>");
		else if (en) {
			printf("+");
			active[j++] = i;
		} else
			printf("-");

		if (i == last_num)
			printf("\n");
		else
			printf(" ");
	}

	dump_dp_attributes(router, active);
}

/* Dumps the verbose output for DP IN adapters */
static void dump_dp_in_adapters(const char *router)
{
	u8 active[MAX_ADAPTERS];
	bool found = false;
	int i, last_num;
	u8 num, j;
	u64 en;

	num = get_dp_adps_num(router);
	if (num == MAX_ADAPTERS) {
		dump_spaces(VERBOSE_L2_SPACES);
		printf("DP IN: <Not accessible>\n");

		return;
	}

	for (i = MAX_ADAPTERS - 1; i >=0; i--) {
		if (is_adp_dp_in(router, i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));;
	j = 0;

	for (i = 0; i < MAX_ADAPTERS; i++) {
		if (!is_adp_dp_in(router, i))
			continue;

		if (!found) {
			dump_spaces(VERBOSE_L2_SPACES);
			printf("DP IN: ");

			found = true;
		}

		printf("%u", i);

		en = is_dp_adp_enabled(router, i);
		if (en == MAX_BIT32)
			printf("<Not accessible>");
		else if (en) {
			printf("+");
			active[j++] = i;
		} else
			printf("-");

		if (i == last_num)
			printf("\n");
		else
			printf(" ");
	}

	dump_dp_attributes(router, active);
}

/*
 * Dumps the verbose output for lane adapters including:
 * 1. Lock status of the adapter
 * 2. Enablement of the hot events on the adapter
 * 3. CLx support on the adapter
 */
static void dump_lane_adapters(const char *router)
{
	u32 usb4_clx, en_cl0s, en_cl1, en_cl2;
	u64 locked, usb4_dh, clx;
	u8 i = 0, majv;
	u16 usb4v, dh;

	for (; i < MAX_ADAPTERS - 1; i++) {
		if (!is_adp_lane(router, i))
			continue;

		dump_spaces(VERBOSE_L2_SPACES);

		printf("Port %u: ", i);

		locked = is_adp_locked(router, i);
		if (locked == MAX_BIT32)
			printf("Locked: <Not accessible>\n");
		else if (locked)
			printf("Locked: yes\n");
		else
			printf("Locked: no\n");

		dump_spaces(VERBOSE_L3_SPACES);

		usb4v = get_usb4v(router);
		if (usb4v == MAX_BIT8)
			printf("Hot events: <Not accessible>\n");

		majv = (usb4v & USB4V_MAJOR_VER) >> USB4V_MAJOR_VER_SHIFT;
		if (!majv) {
			dh = is_tbt3_hot_events_disabled_lane(router);
			if (dh == MAX_BIT8)
				printf("Hot events: <Not accessible>\n");
			else if (dh)
				printf("Hot events: disabled\n");
			else
				printf("Hot events: enabled\n");
		} else {
			usb4_dh = are_hot_events_disabled(router, i);
			if (usb4_dh == MAX_BIT32)
				printf("Hot events: <Not accessible>\n");
			else if (usb4_dh)
				printf("Hot events: disabled\n");
			else
				printf("Hot events: enabled\n");
		}

		if (usb4v == MAX_BIT8) {
			dump_spaces(VERBOSE_L3_SPACES);
			printf("CLx support: <Not accessible>\n");
		}

		if (!majv) {
			dump_spaces(VERBOSE_L3_SPACES);

			clx = is_tbt3_clx_supported(router,
						    get_usb4_port_num(i));
			if (clx == MAX_BIT32)
				printf("CLx support: <Not accessible>\n");
			else if (clx) {
				printf("CLx support: Pres+\n");

				dump_spaces(VERBOSE_L3_SPACES);

				en_cl0s = are_cl0s_enabled(router, i);
				if (en_cl0s == MAX_BIT16)
					printf("CL0s: <Not accessible>\n");
				else if (en_cl0s)
					printf("CL0s: En+\n");
				else
					printf("CL0s: En-\n");

				dump_spaces(VERBOSE_L3_SPACES);

				en_cl1 = is_cl1_enabled(router, i);
				if (en_cl1 == MAX_BIT16)
					printf("CL1: <Not accessible>\n");
				else if (en_cl1)
					printf("CL1: En+\n");
				else
					printf("CL1: En-\n");

				dump_spaces(VERBOSE_L3_SPACES);

				en_cl2 = is_cl2_enabled(router, i);
				if (en_cl2 == MAX_BIT16)
					printf("CL2: <Not accessible>\n");
				else if (en_cl2)
					printf("CL2: En+\n");
				else
					printf("CL2: En-\n");
			} else
				printf("CLx support: Pres-\n");
		} else if (is_adp_lane_0(router, i)) {
			dump_spaces(VERBOSE_L3_SPACES);

			usb4_clx = is_usb4_clx_supported(router, i);
			if (usb4_clx == MAX_BIT16)
				printf("CLx support: <Not accessible>\n");
			else if (usb4_clx) {
				printf("CLx support: Pres+\n");

				dump_spaces(VERBOSE_L3_SPACES);

				en_cl0s = are_cl0s_enabled(router, i);
				if (en_cl0s == MAX_BIT16)
					printf("CL0s: <Not accessible>\n");
				else if (en_cl0s)
					printf("CL0s: En+\n");
				else
					printf("CL0s: En-\n");

				dump_spaces(VERBOSE_L3_SPACES);

				en_cl1 = is_cl1_enabled(router, i);
				if (en_cl1 == MAX_BIT16)
					printf("CL1: <Not accessible>\n");
				else if (en_cl1)
					printf("CL1: En+\n");
				else
					printf("CL1: En-\n");

				dump_spaces(VERBOSE_L3_SPACES);

				en_cl2 = is_cl2_enabled(router, i);
				if (en_cl2 == MAX_BIT16)
					printf("CL2: <Not accessible>\n");
				else if (en_cl2)
					printf("CL2: En+\n");
				else
					printf("CL2: En-\n");
			} else
				printf("CLx support: Pres-\n");
		}
	}
}

static bool dump_router_verbose(char *router, u8 num)
{
	u64 topid_low, topid_high;
	u8 max_adp, majv;
	char *route_str;
	u16 usb4v;

	are_adp_types_filled = false;
	fill_adp_types_in_router(router);

	topid_low = get_top_id_low(router);
	if (topid_low == MAX_BIT32)
		return false;

	topid_high = get_top_id_high(router);
	if (topid_high == MAX_BIT32)
		return false;

	route_str = get_route_string(topid_high << 23 | topid_low);
	printf("%s ", route_str);
	free(route_str);

	dump_name(router);

	dump_generation(router);

	dump_spaces(VERBOSE_L1_SPACES);

	if (!is_host_router(router)) {
		dump_nvm_version(router);

		dump_lanes(router);
		printf("/");
		dump_speed(router);
	}

	dump_auth_sts(router);

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Domain: %u Depth: %u\n", domain_of_router(router), depth_of_router(router));

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Max adapter num: ");

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		printf("<Not accessible>\n");
	else
		printf("%u\n", max_adp);

	dump_spaces(VERBOSE_L1_SPACES);
	printf("State: %s\n", get_router_state(router));

	/* Dump notification timeout in case of high verbosity */
	if (num > 1)
		dump_not_timeout(router);

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Capabilities: Compatibility\n");

	if (num > 1) {
		dump_tbt_compatibility(router);
		dump_power_states_compatibility(router);
	}

	if (!is_host_router(router)) {
		dump_spaces(VERBOSE_L1_SPACES);
		printf("Capabilities: Controllers\n");

		if (num > 1)
			dump_ihci_status(router);

		dump_spaces(VERBOSE_L1_SPACES);
		printf("Capabilities: Tunneling\n");

		if (num > 1) {
			dump_pcie_tunneling_status(router);
			dump_usb3_tunneling_status(router);
		}
	}

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Capabilities: Wakes\n");

	if (num > 1)
		dump_wakes(router);

	usb4v = get_usb4v(router);
	if (usb4v == MAX_BIT8)
		majv = 0;
	else
		majv = (usb4v & USB4V_MAJOR_VER) >> USB4V_MAJOR_VER_SHIFT;

	if (!is_host_router(router) || majv) {
		dump_spaces(VERBOSE_L1_SPACES);
		printf("Capabilities: Wake status\n");

		if (num > 1)
			dump_wake_status(router);
	}

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Capabilities: Lane adapters\n");

	if (num > 1)
		dump_lane_adapters(router);

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Capabilities: Protocol adapters\n");

	dump_adapters_num(router);

	if (num > 1) {
		dump_up_usb3_adapters(router);
		dump_down_usb3_adapters(router);

		dump_up_pcie_adapters(router);
		dump_down_pcie_adapters(router);

		dump_dp_in_adapters(router);
		dump_dp_out_adapters(router);
	}

	return true;
}

static bool dump_domain_verbose(u8 domain, char *depth, u8 num)
{
	struct list_item *router, *head;
	char path[MAX_LEN];
	bool found = false;

	snprintf(path, sizeof(path), "for line in $(ls %s); do echo $line; done",
		 tbt_sysfs_path);

	router = do_bash_cmd_list(path);
	head = router;

	if (depth) {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			if (is_router_depth((char*)router->val, strtoud(depth)))
				found |= dump_router_verbose((char*)router->val, num);
		}
	} else {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			found |= dump_router_verbose((char*)router->val, num);
		}
	}

	free_list(head);

	return found;
}

/*
 * Function to be called with '-v' as the only additional argument.
 *
 * @num: Indicates the number of 'v' provided as the argument (caps to 'vv').
 */
int lstbt_v(char *domain, char *depth, char *device, u8 num)
{
	u8 domains = total_domains();
	bool found = false;
	u8 i;

	if (!domains) {
		fprintf(stderr, "thunderbolt can't be found\n");
		return 1;
	}

	if (!validate_args(domain, depth, device)) {
		fprintf(stderr, "invalid argument(s)\n%s", help_msg);
		return 1;
	}

	if (device) {
		if (!is_router_present(device)) {
			fprintf(stderr, "invalid device\n");
			return 1;
		}

		found = dump_router_verbose(device, num);
		if (!found)
			fprintf(stderr, "no routers found/accessible\n");

		return 0;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found |= dump_domain_verbose(i, NULL, num);
	} else if (domain && !depth) {
		found = dump_domain_verbose(strtoud(domain), NULL, num);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found |= dump_domain_verbose(i, depth, num);
	} else
		found = dump_domain_verbose(strtoud(domain), depth, num);

	if (!found)
		fprintf(stderr, "no routers found/accessible\n");

	return 0;
}
