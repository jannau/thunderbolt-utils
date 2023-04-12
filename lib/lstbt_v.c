#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "helpers.h"
#include "adapter.h"
#include "router.h"

#define VERBOSE_L1_SPACES		21
#define VERBOSE_L2_SPACES		29
#define VERBOSE_L3_SPACES		37

/* Dump the vendor/device name of the router */
static void dump_name(const char *router)
{
	char v_path[MAX_LEN], d_path[MAX_LEN];
	char *vendor, *device;

	snprintf(v_path, sizeof(v_path), "cat %s%s/vendor_name", tbt_sysfs_path, router);
	vendor = do_bash_cmd(v_path);

	snprintf(d_path, sizeof(d_path), "cat %s%s/device_name", tbt_sysfs_path, router);
	device = do_bash_cmd(d_path);

	printf("%s %s ", vendor, device);
}

static void dump_spaces(u64 spaces)
{
	while (spaces--)
		printf(" ");
}

/*
 * Returns the router state respectively among one of the following:
 * 1. Uninitialized unplugged
 * 2. Uninitialized plugged
 * 3. Enumerated
 */
static char* get_router_state(const char *router)
{
	u64 plugged, configured;
	u8 upstream_adp;

	configured = is_router_configured(router);

	if (is_host_router(router)) {
		if (configured == MAX_BIT32)
			return "<Not accessible>";
		else if (configured)
			return "Enumerated";
		else
			return "Uninitialized plugged";
	}

	upstream_adp = get_upstream_adp(router);
	if (upstream_adp == MAX_ADAPTERS)
		return "<Not accessible>";

	plugged = is_adp_plugged(router, upstream_adp);
	if (plugged == MAX_BIT32)
		return "<Not accessible>";
	else if (!plugged)
		return "Uninitialized unplugged";
	else {
		if (configured == MAX_BIT32)
			return "<Not accessible>";
		else if (configured)
			return "Enumerated";
		else
			return "Uninitialized plugged";
	}
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
static char* get_upstream_router(const char *router)
{
	char *ups_router = malloc(MAX_LEN * sizeof(char));
	char path[MAX_LEN];
	u64 pos1, pos2;
	char *output;

	if (is_host_router(router))
		return router;

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
	u8 depth;

	if (is_host_router(router))
		return 0;

	topid_low = get_top_id_low(router);
	if (topid_low == MAX_BIT32)
		return MAX_ADAPTERS;

	topid_high = get_top_id_high(router);
	if (topid_high == MAX_BIT32)
		return MAX_ADAPTERS;

	top_id = topid_high << 23 | topid_low;

	depth = depth_of_router(router) - 1;

	level_bitmask = map_lvl_to_bitmask(depth);
	return (top_id & level_bitmask) >> (8 * depth);
}


/* Dump the power-states (sleep as of now) support for the router */
static void dump_power_states_compatibility(const char *router)
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

	usb4v = get_usb4v(router);
	if (usb4v == MAX_BIT8) {
		printf("<Not accessible>\n");
		return;
	}

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
	u64 pcie;
	u64 cv, cr;

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
	u64 usb3;
	u64 cv, cr;

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
 * Caller needs to ensure the parameter passed is a TBT3 router.
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
 * Caller needs to ensure the parameter passed is a TBT3 router.
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
	if (wakes == MAX_BIT8)
		printf("<Not accessible>\n");
	else if (wakes)
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
 * This can be used for both USB4 and TBT3 routers.
 */
static void dump_gen_wake_status(const char *router)
{
	u16 sts;

	dump_spaces(VERBOSE_L2_SPACES);
	printf("Wake indication: ");

	sts = get_wake_status(router, PROTOCOL_PCIE);
	if (sts == MAX_BIT8)
		printf("<Not accessible>\n");
	else if (sts)
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
 * This is only applicable for USB4 routers.
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
		printf("<Not accessible>\n");

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
	u8 i = 0, count = 0;
	u8 max_adp;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		return 0;

	for (; i <= max_adp; i++) {
		if (is_adp_usb3(router, i))
			count++;
	}

	return count;
}


/* Returns the total no. of PCIe adapters */
static u8 get_pcie_adps_num(const char *router)
{
	u8 i = 0, count = 0;
	u8 max_adp;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		return 0;

	for (; i <= max_adp; i++) {
		if (is_adp_pcie(router, i))
			count++;
	}

	return count;
}

/* Returns the total no. of DP adapters */
static u8 get_dp_adps_num(const char *router)
{
	u8 i = 0, count = 0;
	u8 max_adp;

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		return 0;

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
	printf("USB3:%u PCIe:%u DP:%u\n", usb3, pcie, dp);
}

static u32 usb3_bw_to_mbps(u32 bw, u16 scale)
{
	u32 mbps;

	mbps = (((bw * 512) << scale) * 8000) / (1000 * 1000);
	return round(mbps);
}

/*
 * Dumps the consumed and allocated upstream/downstream bandwidths of the
 * provided USB3 adapters (only applicable for host routers), and their
 * actual and max. supported link rates.
 *
 * @active: Array containing the active USB3 adapter numbers in the router.
 */
static void dump_usb3_bws_lr(const char *router, u8 active[])
{
	u16 scale, ulv, alr, mlr;
	u32 cub, cdb, aub, adb;
	u8 i = 0;

	for (; i < MAX_ADAPTERS; i++) {
		bool alr_dump = false;
		bool host = false;
		char num[MAX_LEN];
		u64 spaces;

		if (!active[i])
			return;

		dump_spaces(VERBOSE_L3_SPACES);
		printf("%u: ", active[i]);

		snprintf(num, sizeof(num), "%u: ", active[i]);
		spaces = strlen(num);

		if (is_host_router(router)) {
			host = true;

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
		}

		ulv = is_usb3_link_valid(router, active[i]);
		alr = get_usb3_actual_lr(router, active[i]);
		mlr = get_usb3_max_sup_lr(router, active[i]);

		if (host)
			dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (ulv == MAX_BIT8 || alr == MAX_BIT8);
		else if (ulv && alr == USB3_LR_GEN2_SL) {
			printf("Actual link rate: 10Gbps\n");
			alr_dump = true;
		} else if (ulv && alr == USB3_LR_GEN2_DL) {
			printf("Actual link rate: 20Gbps\n");
			alr_dump = true;
		}

		if (alr_dump)
			dump_spaces(VERBOSE_L3_SPACES + spaces);

		if (mlr == MAX_BIT8)
			printf("Max. supported link rate: <Not accessible>\n");
		else if (mlr == USB3_LR_GEN2_SL)
			printf("Max. supported link rate: 10Gbps\n");
		else if (mlr == USB3_LR_GEN2_DL)
			printf("Max. supported link rate: 20Gbps\n");
	}
}

/* Dumps the verbose output for downstream USB3 adapters in the router */
static void dump_down_usb3_adapters(const char *router)
{
	u8 num, max_adp, last_num, i, j;
	u8 active[MAX_ADAPTERS];
	bool found = false;
	u64 en;

	num = get_usb3_adps_num(router);
	if (!num)
		return;

	max_adp = get_max_adp(router);

	for (i = max_adp; i >=0; i--) {
		if (is_adp_down_usb3(router ,i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));
	j = 0;

	for (i = 0; i <= max_adp; i++) {
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

	dump_usb3_bws_lr(router, active);
}

/* Dumps the verbose output for upstream USB3 adapters in the router */
static void dump_up_usb3_adapters(const char *router)
{
	u8 num, max_adp, last_num, i, j;
	u8 active[MAX_ADAPTERS];
	bool found = false;
	u64 en;

	num = get_usb3_adps_num(router);
	if (!num)
		return;

	max_adp = get_max_adp(router);

	for (i = max_adp; i >=0; i--) {
		if (is_adp_up_usb3(router ,i))
			break;
	}
	last_num = i;

	if (last_num < 0)
		return;

	memset(active, 0, MAX_ADAPTERS * sizeof(u8));
	j = 0;

	for (i = 0; i <= max_adp; i++) {
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

	dump_usb3_bws_lr(router, active);
}

static bool dump_router_verbose(const char *router, u8 num)
{
	u64 topid_low, topid_high;
	u8 max_adp, majv;
	u16 usb4v;

	topid_low = get_top_id_low(router);
	if (topid_low == MAX_BIT32)
		return false;

	topid_high = get_top_id_high(router);
	if (topid_high == MAX_BIT32)
		return false;

	printf("%s ", get_route_string(topid_high << 23 | topid_low));

	dump_name(router);

	dump_generation(router);

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Domain: %u Depth: %u\n", domain_of_router(router), depth_of_router(router));

	dump_spaces(VERBOSE_L1_SPACES);
	printf("Total adapters: ");

	max_adp = get_max_adp(router);
	if (max_adp == MAX_ADAPTERS)
		printf("<Not accessible>\n");
	else
		printf("%u\n", max_adp + 1);

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
	printf("Capabilities: Protocol adapters\n");

	dump_adapters_num(router);

	if (num > 1) {
		dump_up_usb3_adapters(router);
		dump_down_usb3_adapters(router);
	}

	return true;
}

static bool dump_domain_verbose(u8 domain, const u8 *depth, u8 num)
{
	struct list_item *router;
	char path[MAX_LEN];
	bool found = false;

	snprintf(path, sizeof(path), "for line in $(ls %s); do echo $line; done",
		 tbt_sysfs_path);

	router = do_bash_cmd_list(path);

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

	return found;
}

/*
 * Function to be called with '-v' as the only additional argument.
 * @num: Indicates the number of 'v' provided as the argument (caps to 'vv').
 */
void lstbt_v(const u8 *domain, const u8 *depth, const char *device, u8 num)
{
	u8 domains = total_domains();
	bool found = false;
	u8 i;

	if (!domains) {
		fprintf(stderr, "thunderbolt can't be found\n");
		return;
	}

	if (!validate_args(domain, depth, device)) {
		fprintf(stderr, "invalid argument(s)\n");
		return;
	}

	if (device) {
		if (!is_router_present(device)) {
			fprintf(stderr, "invalid device\n");
			return;
		}

		dump_router_verbose(device, num);
		return;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found = dump_domain_verbose(i, NULL, num);
	} else if (domain && !depth) {
		found = dump_domain_verbose(strtoud(domain), NULL, num);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found = dump_domain_verbose(i, depth, num);
	} else
		found = dump_domain_verbose(strtoud(domain), depth, num);

	if (!found)
		printf("no device(s) found\n");
}

int main(void)
{
	debugfs_config_init();
	/*printf("%x\n", get_router_register_val("0-0", 5, 6, 170));
	printf("%x\n", get_adapter_register_val("0-0", 0, 0, 1, 2));
	return 0;*/
	lstbt_v(NULL, NULL, NULL, 1);
	return 0;
}
