#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
	u8 total_adp, port;
	u32 wakes;
	u16 usb4v;
	u8 i = 0;
	u8 majv;

	total_adp = get_total_adp(router);
	if (total_adp == MAX_ADAPTERS) {
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
		for (; i < total_adp; i++) {
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

		for (; i < total_adp; i++) {
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
	u8 total_adp;
	u64 status;
	u16 usb4v;
	u8 i = 0;
	u8 majv;

	total_adp = get_total_adp(router);
	if (total_adp == MAX_ADAPTERS) {
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
		for (; i < total_adp; i++) {
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

static bool dump_router_verbose(const char *router, u8 num)
{
	u64 topid_low, topid_high;
	u8 total_adp, majv;
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

	total_adp = get_total_adp(router);
	if (total_adp == MAX_ADAPTERS)
		printf("<Not accessible>\n");
	else
		printf("%u\n", total_adp);

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
