#include <string.h>
#include <stdio.h>

#include "helpers.h"
#include "adapter.h"

#define LANE_SPEED_GEN3			BIT(18)
#define LANE_SPEED_GEN2			BIT(19)

#define WIDTH_X1			BIT(20)
#define WIDTH_X2			BIT(21)

#define LANE_ADP_STATE_DISABLED		0x0
#define LANE_ADP_STATE_TRAINING		0x1
#define LANE_ADP_STATE_CL0		0x2
#define LANE_ADP_STATE_TRANS_CL0S	0x3
#define LANE_ADP_STATE_RECEIVE_CL0S	0x4
#define LANE_ADP_STATE_CL1		0x5
#define LANE_ADP_STATE_CL2		0x6
#define LANE_ADP_STATE_CLD		0x7

#define CABLE_VER_MAJ_USB4		0x1
#define CABLE_VER_MAJ_TBT3		0x0

/*
 * Returns the register value in the config. space of the provided router having
 * the provided cap. ID and VSEC cap. ID, at the given offset.
 *
 * @cap_id: Capability ID of the desired space.
 * @vcap_id: VSEC ID of the desired space.
 * @adp: Adapter no.
 * @off: Offset (in dwords).
 *
 * Returns (u64)~0 if the register is not accessible.
 */
static u64 get_register_val(const char *router, u8 cap_id, u8 vcap_id, u8 adp, u64 off)
{
	char final_path[MAX_LEN];
	char regs_path[MAX_LEN];
	char path[MAX_LEN];
	char *root_cmd;
	char *output;

	snprintf(regs_path, sizeof(regs_path), "%s/port%u", router, adp);

	snprintf(path, sizeof(path), "%s | %s | %s | %s", cfg_space_tab, cap_vcap_search,
		 print_row_num, print_col_in_row);
	snprintf(final_path, sizeof(final_path), path, tbt_debugfs_path, regs_path, cap_id,
		 vcap_id, off + 1);

	root_cmd = switch_cmd_to_root(final_path);

	output = do_bash_cmd(root_cmd);
	if (!strlen(output))
		return COMPLEMENT_BIT64;

	return strtouh(output);
}

/*
 * Returns 'true' if the adapter is present in the router (more precisely,
 * if the adapter's debugfs is present under the provided router).
 */
bool is_adp_present(const char *router, u8 adp)
{
	char path[MAX_LEN];
	char *root_cmd;
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s%s/port%u | wc -l", tbt_debugfs_path,
		 router, adp);
	root_cmd = switch_cmd_to_root(path);

	output = do_bash_cmd(root_cmd);
	if (strtoud(output))
		return true;

	return false;
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

	val = get_register_val(router, 0, 0, adp, ADP_CS_2);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val & ADP_CS_2_PVS;
}

/*
 * Returns '1' if the adapter is plugged, '0' otherwise.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 256.
 */
u16 is_adp_plugged(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, 0, 0, adp, ADP_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_CS_4_PLUGGED) >> ADP_CS_4_PLUGGED_SHIFT;
}

/*
 * Returns '1' if control packets can be forwarded via this adapter, '0'
 * otherwise.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 256.
 */
u16 is_adp_locked(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, 0, 0, adp, ADP_CS_4);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_CS_4_LOCK) >> ADP_CS_4_LOCK_SHIFT;
}

/*
 * Returns '1' if hot events are disabled on this adapter, '0' otherwise.
 * If config. space is inaccessible or adapter doesn't exist, return a
 * value of 256.
 */
u16 are_hot_events_disabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, 0, 0, adp, ADP_CS_5);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & ADP_CS_5_DHP) >> ADP_CS_5_DHP_SHIFT;
}

/*
 * Returns 'true' if the adapter is a lane adapter, 'false' otherwise.
 * Caller needs to make sure that the adapter no. is valid.
 */
bool is_adp_lane(const char *router, u8 adp)
{
	u64 pvs;

	pvs = get_adp_pvs(router, adp);
	if (pvs == MAX_BIT32)
		return false;

	return pvs == LANE_PVS;
}

/*
 * Fetch supported link speeds on the given lane adapter.
 * Check the SPEED_GENX definitions in the file.
 * Return a value of 256 on any error.
 */
u16 get_sup_link_speeds(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_SUP_SPEEDS) >> LANE_ADP_CS_0_SUP_SPEEDS_SHIFT;
}

/*
 * Fetch supported link widths on the given lane adapter.
 * Check the WIDTH_XX definitions in the file.
 * Return a value of 256 on any error.
 */
u16 get_sup_link_widths(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_SUP_WIDTH) >> LANE_ADP_CS_0_SUP_WIDTH_SHIFT;
}

/*
 * Returns '1' if CL0s are supported on the given lane, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 are_cl0s_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_CL0S_SUP) >> LANE_ADP_CS_0_CL0S_SUP_SHIFT;
}

/*
 * Returns '1' if CL1 is supported on the given lane, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_cl1_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_CL1_SUP) >> LANE_ADP_CS_0_CL1_SUP_SHIFT;
}

/*
 * Returns '1' if CL2 is supported on the given lane, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_cl2_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_0);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_0_CL2_SUP) >> LANE_ADP_CS_0_CL2_SUP_SHIFT;
}

/*
 * Returns '1' is CL0s are enabled on the given lane, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 are_cl0s_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_CL0S_EN) >> LANE_ADP_CS_1_CL0S_EN_SHIFT;
}

/*
 * Returns '1' if CL1 is enabled on the given lane, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_cl1_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_CL1_EN) >> LANE_ADP_CS_1_CL1_EN_SHIFT;
}

/*
 * Returns '1' if CL2 is enabled on the given lane, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_cl2_enabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_CL2_EN) >> LANE_ADP_CS_1_CL2_EN_SHIFT;
}

/*
 * Returns '1' if the lane is disabled, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_lane_disabled(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_LD) >> LANE_ADP_CS_1_LD_SHIFT;
}

/*
 * Fetch current link speeds on the given lane adapter.
 * Check the SPEED_GENX definitions in the file.
 * Return a value of 256 on any error.
 */
u16 cur_link_speed(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & LANE_ADP_CS_1_CUR_LINK_SPEED;
}

/*
 * Fetch negotiated link widths on the given lane adapter.
 * Check the WIDTH_XX definitions in the file.
 * Return a value of 256 on any error.
 */
u16 neg_link_width(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return val & LANE_ADP_CS_1_NEG_LINK_WIDTH;
}

/*
 * Fetch the lane adapter state.
 * Check the LANE_ADP_STATE_X definitions in the file.
 * Return a value of 256 on any error.
 */
u16 get_lane_adp_state(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_ADP_STATE) >> LANE_ADP_CS_1_ADP_STATE_SHIFT;
}

/*
 * Returns '1' if the lane adapter is PM secondary, '0' otherwise.
 * Return a value of 256 on any error.
 */
u16 is_secondary_lane_adp(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, LANE_ADP_CAP_ID, 0, adp, LANE_ADP_CS_1);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & LANE_ADP_CS_1_PMS) >> LANE_ADP_CS_1_PMS_SHIFT;
}


/* Returns 'true' if the given adapter is Lane-0 adapter, 'false' otherwise */
bool is_adp_lane_0(const char *router, u8 adp)
{
	if (!is_adp_lane(router, adp))
		return false;

	return (adp % 2) != 0;
}

/*
 * Returns the USB4 version supported by the type-C cable plugged in.
 * Check CABLE_VER_MAJ_X definitions in the file.
 * Return a value of 2^16 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u32 get_usb4_cable_version(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT16;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT16;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT16;

	return (val & PORT_CS_18_CUSB4_VER_MAJ) >> PORT_CS_18_CUSB4_VER_MAJ_SHIFT;
}

/*
 * Returns '1' if conditions for lane bonding are met on the respective port,
 * '0' otherwise.
 * Return a value of 256 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u16 is_usb4_bonding_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & PORT_CS_18_BE) >> PORT_CS_18_BE_SHIFT;
}

/*
 * Returns '1' if the link is operating in TBT3 mode, '0' otherwise.
 * Return a value of 256 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u16 is_usb4_tbt3_compatible_mode(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & PORT_CS_18_TCM) >> PORT_CS_18_TCM_SHIFT;
}

/*
 * Returns '1' if CLx is supported on the lane, '0' otherwise.
 * Return a value of 256 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u16 is_usb4_clx_supported(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & PORT_CS_18_CPS) >> PORT_CS_18_CPS_SHIFT;
}

/*
 * Returns '1' if a router is detected on the port, '0' otherwise.
 * Return a value of 256 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u16 is_usb4_router_detected(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & PORT_CS_18_RD) >> PORT_CS_18_RD_SHIFT;
}

/*
 * Returns the wake status on the USB4 port.
 * Check PORT_CS_18_WX definitions in the file.
 * Return a value of 2^32 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u64 get_usb4_wake_status(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT32;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_18);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val;
}

/*
 * Returns '1' if the USB4 port is configured, '0' otherwise.
 * Return a value of 256 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u16 is_usb4_port_configured(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT8;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT8;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_19);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT8;

	return (val & PORT_CS_19_PC) >> PORT_CS_19_PC_SHIFT;
}

/*
 * Returns the wake events enabled on the USB4 port.
 * Check PORT_CS_19_EX definitions in the file.
 * Return a value of 2^32 on any error.
 *
 * NOTE: This is only applicable for Lane-0 adapter on a USB4 port.
 */
u64 get_usb4_wakes_en(const char *router, u8 adp)
{
	u64 val;

	if (!is_adp_present(router, adp))
		return MAX_BIT32;

	if (!is_adp_lane_0(router, adp))
		return MAX_BIT32;

	val = get_register_val(router, USB4_PORT_CAP_ID, 0, adp, PORT_CS_19);
	if (val == COMPLEMENT_BIT64)
		return MAX_BIT32;

	return val;
}

int main(void)
{
	char *router = "0-1";
	printf("%x\n", get_register_val(router, 0, 0, 1, ADP_CS_2));
	printf("%d\n", is_adp_present(router, 0));
	printf("%x\n", get_adp_pvs(router, 11));
	printf("%d\n", are_hot_events_disabled(router, 11));
	printf("%x\n", get_sup_link_speeds(router, 1));
	printf("%x\n", get_sup_link_widths(router, 1));
	printf("%x\n", get_usb4_cable_version(router, 1));
	printf("is:%x\n", is_usb4_tbt3_compatible_mode(router, 1));
	return 0;
}
