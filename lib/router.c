#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "helpers.h"
#include "router.h"

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
 * Check for 'get_tbt3_wake_events_en' definition below.
 */
#define TBT3_HOT_PLUG_ROUTER	BIT(1)
#define TBT3_HOT_UNPLUG_ROUTER	BIT(2)
#define TBT3_HOT_PLUG_DP	BIT(3)
#define TBT3_HOT_UNPLUG_DP	BIT(4)
#define TBT3_USB4		BIT(5)
#define TBT3_PCIE		BIT(6)
#define TBT3_HOT_PLUG_USB	BIT(9)
#define TBT3_HOT_UNPLUG_USB	BIT(10)

/*
 * Returns the register value in the config. space of the provided router having
 * the provided cap. ID and VSEC cap. ID, at the given offset.
 *
 * @cap_id: Capability ID of the desired space.
 * @vcap_id: VSEC ID of the desired space.
 * @off: Offset (in dwords).
 *
 * Returns (u64)~0 if the register is not accessible.
 */
static u64 get_register_val(const char *router, u8 cap_id, u8 vcap_id, u64 off)
{
	char final_path[MAX_LEN];
	char path[MAX_LEN];
	char *root_cmd;
	char *output;

	snprintf(path, sizeof(path), "%s | %s | %s | %s", cfg_space_tab, cap_vcap_search,
		 print_row_num, print_col_in_row);
	snprintf(final_path, sizeof(final_path), path, tbt_debugfs_path, router, cap_id,
		 vcap_id, off + 1);

	root_cmd = switch_cmd_to_root(final_path);

	output = do_bash_cmd(root_cmd);
	if (!strlen(output))
		return COMPLEMENT_BIT64;

	return strtouh(output);
}

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

	val = get_register_val(router, 0, 0, ROUTER_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_ADAPTERS;

	return (val & ROUTER_CS_1_UPS_ADP) >> ROUTER_CS_1_UPS_ADP_SHIFT;
}

/*
 * Find the total no. of adapters in the router.
 * If config. space is inaccessible, return a value of 64.
 */
u8 get_total_adp(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_ADAPTERS;

	return ((val & ROUTER_CS_1_MAX_ADP) >> ROUTER_CS_1_MAX_ADP_SHIFT) + 1;
}

/*
 * Return the least significant 32 bits of the topology ID.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 get_top_id_low(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val;
}

/*
 * Return the most significant 24 bits of the router.
 * If config. space is inaccessible, return a value of 2^32.
 */
u64 get_top_id_high(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_3);
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

	val = get_register_val(router, 0, 0, ROUTER_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_1_REV_NO) >> ROUTER_CS_1_REV_NO_SHIFT;
}

/*
 * Returns '1' if the router is configured (top. ID is valid), '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_router_configured(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_3);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_3_TOP_ID_VALID) >> ROUTER_CS_3_TOP_ID_VALID_SHIFT;
}

/*
 * Returns the timeout configured to send the hot event again if CM fails to ack.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_notification_timeout(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_4);
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

	val = get_register_val(router, 0, 0, ROUTER_CS_4);
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

	val = get_register_val(router, 0, 0, ROUTER_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_4_USB4V) >> ROUTER_CS_4_USB4V_SHIFT;
}

/*
 * Returns '1' if the wakes are enabled on the provided protocol, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_wake_enabled(const char *router, u8 protocol)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	if (protocol == PROTOCOL_PCIE)
		return (val & ROUTER_CS_5_WOP) >> ROUTER_CS_5_WOP_SHIFT;
	else if (protocol == PROTOCOL_USB3)
		return (val & ROUTER_CS_5_WOU) >> ROUTER_CS_5_WOU_SHIFT;
	else
		return (val & ROUTER_CS_5_WOD) >> ROUTER_CS_5_WOD_SHIFT;
}

/*
 * Returns '1' if the protocol tunneling is turned on, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_tunneling_on(const char *router, u8 protocol)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	if (protocol == PROTOCOL_PCIE)
		return (val & ROUTER_CS_5_PTO) >> ROUTER_CS_5_PTO_SHIFT;
	else if (protocol == PROTOCOL_USB3)
		return (val & ROUTER_CS_5_UTO) >> ROUTER_CS_5_UTO_SHIFT;
	else if (protocol == PROTOCOL_HCI)
		return (val & ROUTER_CS_5_IHCO) >> ROUTER_CS_5_IHCO_SHIFT;
	else
		return true;
}

/*
 * Returns '1' if tunneling config. is valid, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_tunneling_config_valid(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_5_CV) >> ROUTER_CS_5_CV_SHIFT;
}

/*
 * Returns '1' if router is ready to sleep, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_router_sleep_ready(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_6_SR);
}

/*
 * Returns '1' if a wake was caused by the provided protocol, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 get_wake_status(const char *router, u8 protocol)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	if (protocol == PROTOCOL_PCIE)
		return (val & ROUTER_CS_6_WOPS) >> ROUTER_CS_6_WOPS_SHIFT;
	else if (protocol == PROTOCOL_USB3)
		return (val & ROUTER_CS_6_WOUS) >> ROUTER_CS_6_WOUS_SHIFT;
	else
		return (val & ROUTER_CS_6_WODS) >> ROUTER_CS_6_WODS_SHIFT;
}

/*
 * Returns '1' if internal HCI is present, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_ihci_present(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_6_IHCI) >> ROUTER_CS_6_IHCI_SHIFT;
}

/*
 * Returns '1' if router is ready after CMUV bit gets set to '1', '0'
 * otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_router_ready(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_6_RR) >> ROUTER_CS_6_RR_SHIFT;
}

/*
 * Returns '1' if router is ready for protocol tunneling provided by the CM,
 * '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 */
u16 is_tunneling_ready(const char *router)
{
	u64 val;

	val = get_register_val(router, 0, 0, ROUTER_CS_6);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_CS_6_CR) >> ROUTER_CS_6_CR_SHIFT;
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

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID,
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

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID,
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

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID,
			       ROUTER_VSEC6_COM);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & ROUTER_VSEC6_COM_PORTS;
}

/*
 * Returns '1' if conditions for lane bonding are met, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 is_tbt3_bonding_en(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT8;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT8;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_PORT_ATTR;

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_VSEC6_PORT_ATTR_BE) >> ROUTER_VSEC6_PORT_ATTR_BE_SHIFT;
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

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return val & ROUTER_VSEC6_LC_SX_CTRL_EWE;
}

/*
 * Returns the status of the configuration of lanes 0 and 1.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 get_tbt3_lanes_configured(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT8;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT8;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LC_SX_CTRL;

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & (ROUTER_VSEC6_LC_SX_CTRL_L0C | ROUTER_VSEC6_LC_SX_CTRL_L1C);
}

/*
 * Returns '1' if the link connected to the port is operating in TBT3
 * compatible mode, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 is_tbt3_compatible_mode(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT8;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT8;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LINK_ATTR;

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_VSEC6_LINK_ATTR_TCM) >> ROUTER_VSEC6_LINK_ATTR_TCM_SHIFT;
}

/*
 * Returns '1' if CLx is supported on the lane, '0' otherwise.
 * If config. space is inaccessible, return a value of 256.
 *
 * Valid only for TBT3 routers.
 */
u16 is_tbt3_clx_supported(const char *router, u8 port)
{
	u64 com_len, usb4_len;
	u64 off, val;

	com_len = get_tbt3_com_reg_dwords(router);
	if (com_len == MAX_BIT8)
		return MAX_BIT8;

	usb4_len = get_tbt3_usb4_reg_dwords(router);
	if (usb4_len == MAX_BIT16)
		return MAX_BIT8;

	off = (usb4_len * port) + com_len + ROUTER_VSEC6_LINK_ATTR;

	val = get_register_val(router, ROUTER_VCAP_ID, ROUTER_VSEC6_ID, off);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ROUTER_VSEC6_LINK_ATTR_CPS) >> ROUTER_VSEC6_LINK_ATTR_CPS_SHIFT;
}

int main(void)
{
	u64 low = get_top_id_low("0-30501");
	if (low == MAX_BIT32) {
		fprintf(stderr, "Not accessible\n");
		return 0;
	}
	printf("low:%llx\n", low);
	u64 high = get_top_id_high("0-30501");
	if (high == MAX_BIT32) {
		fprintf(stderr, "Not accessible\n");
                return 0;
        }
	printf("high:%llx\n", high);
	printf("topid:%llx\n", (high << 23) | low);
	printf("routestring:%s\n", get_route_string((high << 23) | low));

	bool a = is_router_configured("0-30501");
	printf("conf:%d\n", a);

	printf("%x\n", get_notification_timeout("0-30501"));

	char *router = "0-30501";
	printf("%x\n", get_cmuv(router));
	printf("%x\n", get_usb4v(router));
	printf("%x\n", is_wake_enabled(router, 0));
	printf("%x\n", is_wake_enabled(router, 2));
	printf("%x\n", is_tunneling_on(router, 1));
	printf("%x\n", get_tbt3_wake_events_en(router, 0));
}
