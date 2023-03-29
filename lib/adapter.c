#include <string.h>
#include <stdio.h>

#include "helpers.h"
#include "adapter.h"

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

int main(void)
{
	char *router = "0-501";
	printf("%x\n", get_register_val(router, 0, 0, 1, ADP_CS_2));
	printf("%d\n", is_adp_present(router, 0));
	printf("%x\n", get_adp_pvs(router, 11));
	printf("%d\n", are_hot_events_disabled(router, 11));
	return 0;
}
