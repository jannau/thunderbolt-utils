// SPDX-License-Identifier: GPL-2.0
/*
 * Helpers for the user-space library (lstbt)
 *
 * This file encompasses various helper functions, mainly for:
 * 1. Common functionalities across the library.
 * 2. The debugfs parameters for faster access.
 * 3. Incorporating the primary function to distribute the functionalities
 *    across the library.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "helpers.h"

/*
 * No. of depths in a domain are constrained due to the limitation in route string
 * storage in the control packets.
 */
#define MAX_DEPTH_POSSIBLE	8

static char *tbt_debugfs_path = "/sys/kernel/debug/thunderbolt/";

static struct router_config *routers_config = NULL;

static char options[] = {'D', 'd', 's', 'r', 't', 'v', 'V', 'h', '\0'};

char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

char *help_msg =
"Usage: lstbt [options]...\n"
"List TBT/USB4 devices\n"
"  -D domain\n"
"      Select the domain lstbt will examine\n"
"  -d depth\n"
"      Select the depth (starting from 0) lstbt will consider\n"
"  -s device\n"
"      Select the device (like displayed in sysfs) lstbt will examine\n"
"  -r retimer\n"
"      Display the retimers present in the thunderbolt subsystem\n"
"  -t tree\n"
"      Display the thunderbolt subsystem in tree format\n"
"  -v verbose\n"
"      Increase the verbosity (-vv for higher)\n"
"  -V version\n"
"      Display the version of the library\n"
"  -h help\n"
"      Display the usage\n";

/*
 * Returns the max. adapter num plus '1', as present in the debugfs of a
 * router.
 */
static u8 get_total_adps_debugfs(const char *router)
{
	struct list_item *item;
	char path[MAX_LEN];
	char *root_cmd;
	u8 port = 63;
	u8 ret = 0;

	snprintf(path, sizeof(path), "ls %s%s | grep 'port'", tbt_debugfs_path,
		 router);
	root_cmd = switch_cmd_to_root(path);

	item = do_bash_cmd_list(root_cmd);

	while (port--) {
		char port_val[MAX_LEN];

		snprintf(port_val, sizeof(port_val), "port%u", port);

		if (is_present_in_list(item, port_val)) {
			ret = port + 1;
			break;
		}
	}

	free_list(item);
	free(root_cmd);

	return ret;
}

static struct list_item* get_cap_vcap_start(struct list_item *regs_list,
					    u8 cap_id, u8 vcap_id)
{
	u64 num_commas, col;
	bool found = false;
	s64 first, second;
	char *regs, *str;
	u64 total_col;

	for (; regs_list; regs_list = regs_list->next) {
		num_commas = 0;
		col = 0;
		regs = (char*)regs_list->val;
		total_col = strlen(regs);

		for (; col < total_col; col++) {
			if (num_commas == 2) {
				u8 cap, vcap;

				first = strpos(regs, ",", col);
				if (first == -1)
					break;

				second = strpos(regs, ",", first + 1);
				if (second == -1)
					break;

				str = get_substr(regs, col, first - col);
				cap = strtouh(str);
				free(str);

				str = get_substr(regs, first + 1,
						 second - first - 1);
				vcap = strtouh(str);
				free(str);

				if (cap == cap_id && vcap == vcap_id)
					found = true;

				break;
			} else if (regs[col] == ',')
				num_commas++;
		}

		if (found)
			return regs_list;
	}

	return NULL;
}

/* Fetches the router config. space of the provided router */
static void get_router_regs(const char *router, struct router_config *config)
{
	struct list_item *item, *router_regs;
	char path[MAX_LEN], check[MAX_LEN];
	char *root_cmd;

	snprintf(check, sizeof(check), "%s%s/regs", tbt_debugfs_path, router);
	if (is_link_nabs(check))
		exit(1);

	snprintf(path, sizeof(path), "cat %s%s/regs | awk -v OFS=',' '{\\$1=\\$1}1'",
		 tbt_debugfs_path, router);
	root_cmd = switch_cmd_to_root(path);

	router_regs = do_bash_cmd_list(root_cmd);
	config->router_regs = router_regs;

	item = get_cap_vcap_start(router_regs, 0x0, 0x0);
	config->regs = list_to_numbered_array(item);

	item = get_cap_vcap_start(router_regs, ROUTER_VCAP_ID,
				  ROUTER_VSEC1_ID);
	config->vsec1_regs = list_to_numbered_array(item);

	item = get_cap_vcap_start(router_regs, ROUTER_VCAP_ID,
				  ROUTER_VSEC3_ID);
	config->vsec3_regs = list_to_numbered_array(item);

	item = get_cap_vcap_start(router_regs, ROUTER_VCAP_ID,
				  ROUTER_VSEC4_ID);
	config->vsec4_regs = list_to_numbered_array(item);

	item = get_cap_vcap_start(router_regs, ROUTER_VCAP_ID,
				  ROUTER_VSEC6_ID);
	config->vsec6_regs = list_to_numbered_array(item);

	free(root_cmd);
}

/*
 * Fetches the adapter config. space of all the ports present in the provided
 * router.
 *
 * NOTE: This assumes that the router has all the ports numbered '1' to the max.
 * adapter num - 1, obtained from 'get_total_adps_debugfs'.
 */
static void get_adps_config(const char *router, struct adp_config *config)
{
	struct list_item *item, *adp_regs;
	u8 total_adps;
	u8 i = 0;

	total_adps = get_total_adps_debugfs(router);

	for (; i < total_adps; i++) {
		char path[MAX_LEN], check[MAX_LEN];
		char *root_cmd;

		config[i].adp = i;

		snprintf(check, sizeof(check), "%s%s/port%u/regs", tbt_debugfs_path,
			 router, i);
		if (is_link_nabs(check))
			exit(1);

		snprintf(path, sizeof(path),
			 "cat 2>/dev/null %s%s/port%u/regs | awk -v OFS=',' '{\\$1=\\$1}1'",
			 tbt_debugfs_path, router, i);
		root_cmd = switch_cmd_to_root(path);

		adp_regs = do_bash_cmd_list(root_cmd);
		config[i].adp_regs = adp_regs;

		item = get_cap_vcap_start(adp_regs, 0, 0);
		config[i].regs = list_to_numbered_array(item);

		item = get_cap_vcap_start(adp_regs,
					  LANE_ADP_CAP_ID, 0);
		config[i].lane_regs = list_to_numbered_array(item);

		item = get_cap_vcap_start(adp_regs,
					  PCIE_ADP_CAP_ID, 0);
		config[i].pcie_regs = list_to_numbered_array(item);

		item = get_cap_vcap_start(adp_regs,
					  DP_ADP_CAP_ID, 0);
		config[i].dp_regs = list_to_numbered_array(item);

		item = get_cap_vcap_start(adp_regs,
					  USB3_ADP_CAP_ID, 0);
		config[i].usb3_regs = list_to_numbered_array(item);

		item = get_cap_vcap_start(adp_regs,
					  USB4_PORT_CAP_ID, 0);
		config[i].usb4_port_regs = list_to_numbered_array(item);

		free(root_cmd);
	}
}

static u64 get_total_routers_in_domain(u8 domain)
{
	struct list_item *router, *head;
	char path[MAX_LEN];
	u64 num = 0;

	snprintf(path, sizeof(path), "for line in $(ls %s); do echo $line; done",
		 tbt_sysfs_path);

	router = do_bash_cmd_list(path);
	head = router;

	for (; router; router = router->next) {
		if (is_router_format((char*)router->val, domain))
			num++;
	}

	free_list(head);

	return num;
}

static struct router_config* get_router_config_item(const char *router)
{
	u64 domains, total_routers = 0;
	u64 i = 0;

	domains = total_domains();
	for (; i < domains; i++)
		total_routers += get_total_routers_in_domain(i);

	for (i = 0; i < total_routers; i++) {
		if (!strcmp(routers_config[i].router, router))
			return &routers_config[i];
	}

	return NULL;
}

static struct adp_config* get_adp_config_item(const char *router,
					      struct adp_config *configs, u8 adp)
{
	u64 i = 0, total_adps;

	total_adps = get_total_adps_debugfs(router);

	for (; i < total_adps; i++) {
		if (configs[i].adp == adp)
			return &configs[i];
	}

	return NULL;
}

/*
 * Returns 'true' if the provided config. space offset string has no valid
 * values (contains 'not accessible').
 *
 * Caller needs to ensure that the argument is valid.
 */
static bool is_offset_inaccessible(const char *regs)
{
	int pos;

	pos = strpos(regs, "not", 0);
	if (pos == -1)
		return false;

	return true;
}

static u64 get_register_val(char **regs_list, u64 off)
{
	u64 total_col;

	if (!regs_list || !(*regs_list))
		return COMPLEMENT_BIT64;

	if (is_offset_inaccessible(regs_list[off]))
		return COMPLEMENT_BIT64;

	total_col = strlen(regs_list[off]);

	return strtouh(regs_list[off] + (total_col - 10));
}

/* Returns 'true' if debugfs in mounted, 'false' otherwise */
static bool is_debugfs_enabled(void)
{
	char path[MAX_LEN];
	char *en;

	snprintf(path, sizeof(path), "mount 2>/dev/null | grep debugfs | wc -l");
	en = do_bash_cmd(path);

	if (!strtoud(en)) {
		free(en);
		return false;
	}

	free(en);
	return true;
}

/* Initialize the debugfs parameters for faster access */
static int debugfs_config_init(void)
{
	struct list_item *router_list, *head;
	char *root_cmd, *router;
	u64 total_routers, i;
	char path[MAX_LEN];
	bool debugfs_en;
	u8 adps;

	debugfs_en = is_debugfs_enabled();
	if (!debugfs_en) {
		fprintf(stderr, "debugfs is not mounted\n");
		return 1;
	}

	snprintf(path, sizeof(path), "ls %s", tbt_debugfs_path);
	root_cmd = switch_cmd_to_root(path);

	router_list = do_bash_cmd_list(root_cmd);
	head = router_list;

	total_routers = get_total_list_items(router_list);
	routers_config = malloc(total_routers * sizeof(struct router_config));

	i = 0;

	for(; router_list; router_list = router_list->next) {
		router = malloc((strlen((char*)router_list->val) + 1) * sizeof(char));
		strcpy(router, (char*)router_list->val);
		router[strlen((char*)router_list->val)] = '\0';

		adps = get_total_adps_debugfs(router);

		routers_config[i].router = router;
		get_router_regs(router, &routers_config[i]);

		routers_config[i].adps_config = malloc(adps *
						       sizeof(struct adp_config));
		get_adps_config(router, routers_config[i].adps_config);

		i++;
	}

	free_list(head);
	free(root_cmd);

	return 0;
}

static void free_router_regs(struct router_config *config, char **regs, u8 cap_id,
			     u8 vcap_id)
{
	struct list_item *item;
	u64 num, i = 0;

	item = get_cap_vcap_start(config->router_regs, cap_id, vcap_id);
	num = get_total_list_items(item);

	for (; i < num; i++)
		free(regs[i]);

	free(regs);
}

static void free_router_config(struct router_config *config)
{
	free(config->router);

	free_router_regs(config, config->regs, 0x0, 0x0);
	free_router_regs(config, config->vsec1_regs, ROUTER_VCAP_ID, ROUTER_VSEC1_ID);
	free_router_regs(config, config->vsec3_regs, ROUTER_VCAP_ID, ROUTER_VSEC3_ID);
	free_router_regs(config, config->vsec4_regs, ROUTER_VCAP_ID, ROUTER_VSEC4_ID);
	free_router_regs(config, config->vsec6_regs, ROUTER_VCAP_ID, ROUTER_VSEC6_ID);

	free_list(config->router_regs);
}

static void free_adp_regs(struct adp_config *config, char **regs, u8 cap_id,
			  u8 vcap_id)
{
	struct list_item *item;
	u64 num, i = 0;

	item = get_cap_vcap_start(config->adp_regs, cap_id, vcap_id);
	num = get_total_list_items(item);

	for (; i < num; i++)
		free(regs[i]);

	free(regs);
}

static void free_adp_config(char *router, struct adp_config *config)
{
	u8 total_adps = get_total_adps_debugfs(router);
	u8 i = 0;

	for (; i < total_adps; i++) {
		free_adp_regs(&config[i], config[i].regs, 0x0, 0x0);
		free_adp_regs(&config[i], config[i].lane_regs, LANE_ADP_CAP_ID, 0x0);
		free_adp_regs(&config[i], config[i].pcie_regs, PCIE_ADP_CAP_ID, 0x0);
		free_adp_regs(&config[i], config[i].dp_regs, DP_ADP_CAP_ID, 0x0);
		free_adp_regs(&config[i], config[i].usb3_regs, USB3_ADP_CAP_ID, 0x0);
		free_adp_regs(&config[i], config[i].usb4_port_regs, USB4_PORT_CAP_ID, 0x0);

		free_list(config[i].adp_regs);
	}

	free(config);
}

/* Free the memory explicitly used for debugfs operations */
static void debugfs_config_exit(void)
{
	struct list_item *router_list;
	u64 total_routers = 0;
	char path[MAX_LEN];
	char *root_cmd;
	u8 i;

	snprintf(path, sizeof(path), "ls %s", tbt_debugfs_path);
	root_cmd = switch_cmd_to_root(path);

	router_list = do_bash_cmd_list(root_cmd);
	total_routers = get_total_list_items(router_list);

	for (i = 0; i < total_routers; i++) {
		free_adp_config(routers_config[i].router, routers_config[i].adps_config);
		free_router_config(&routers_config[i]);
	}

	free(routers_config);

	free_list(router_list);
	free(root_cmd);
}

/*
 * Returns 'true' if the adapter is present in the router (more precisely,
 * if the adapter's debugfs is present under the provided router), 'false'
 * otherwise.
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
	if (strtoud(output)) {
		free(output);
		free(root_cmd);

		return true;
	}

	free(output);
	free(root_cmd);

	return false;
}

/* Returns the total no. of domains in the host */
u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;
	u32 val;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_bash_cmd(path);
	val = strtoud(output);

	free(output);
	return val;
}

/* Validate the arguments for 'lstbt', 'lstbt -t', and 'lstbt -v' */
bool validate_args(char *domain, char *depth, const char *device)
{
	u8 domains = total_domains();
	u8 i = 0;

	if (device) {
		if (domain || depth)
			return false;
		else {
			for (; i < domains; i++) {
				if (is_router_format(device, i))
					return true;
			}

			return false;
		}
	}

	if (domain) {
		if (!isnum(domain) || strtoud(domain) >= domains)
			return false;
	}

	if (depth) {
		if (!isnum(depth) || strtoud(depth) >= MAX_DEPTH_POSSIBLE)
			return false;
	}

	return true;
}

/* Returns 'true' if the router exists, 'false' otherwise */
bool is_router_present(const char *router)
{
	char path[MAX_LEN];
	char *bash;

	snprintf(path, sizeof(path), "ls %s%s %s; echo $?", tbt_sysfs_path,
		 router, REDIRECTED_NULL);
	bash = do_bash_cmd(path);
	if (strtoud(bash)) {
		free(bash);
		return false;
	}

	free(bash);
	return true;
}

/*
 * Check if a given string is in router format.
 * Cases are limited for a thunderbolt directory.
 */
bool is_router_format(const char *router, u8 domain)
{
	char num_str[MAX_LEN];
	char *colon_str = ":";
	int pos, i = 0;

	for (; i < (int)strlen(router); i++) {
		if (!isdigit(router[i]) && (router[i] != '-'))
			return false;
	}

	snprintf(num_str, sizeof(num_str), "%u-", domain);

	pos = strpos(router, num_str, 0);
	if (pos)
		return false;

	pos = strpos(router, colon_str, 0);
	if (pos >= 0)
		return false;

	return true;
}

bool is_host_router(const char *router)
{
	return router[strlen(router)-1] == '0';
}

/*
 * Returns the expected router string length in the given depth.
 * Caller needs to ensure the router string is in correct format.
 */
u8 router_len_in_depth(u8 depth)
{
	if (!depth)
		return 3;

	return ((2 * depth) + 1);
}

/*
 * Returns 'true' if the router is in the given depth.
 * Caller needs to ensure the router string is in correct format.
 */
bool is_router_depth(const char *router, u8 depth)
{
	if (!depth)
		return is_host_router(router);

	return !is_host_router(router) && (strlen(router) == router_len_in_depth(depth));
}

/* Dump the router's vendor/device IDs */
void dump_vdid(const char *router)
{
	char vid_path[MAX_LEN], did_path[MAX_LEN], vcheck[MAX_LEN], dcheck[MAX_LEN];
	char *vid, *did;

	snprintf(vcheck, sizeof(vcheck), "%s%s/vendor", tbt_sysfs_path, router);
	snprintf(dcheck, sizeof(dcheck), "%s%s/device", tbt_sysfs_path, router);
	if (is_link_nabs(vcheck) || is_link_nabs(dcheck))
		exit(1);

	snprintf(vid_path, sizeof(vid_path), "cat %s%s/vendor", tbt_sysfs_path, router);
	vid = do_bash_cmd(vid_path);

	snprintf(did_path, sizeof(did_path), "cat %s%s/device", tbt_sysfs_path, router);
	did = do_bash_cmd(did_path);

	printf("ID %04x:%04x ", strtouh(vid), strtouh(did));

	free(vid);
	free(did);
}

/* Dump the generation of the router */
void dump_generation(const char *router)
{
	char gen[MAX_LEN], check[MAX_LEN];
	u8 generation;
	char *gen_str;

	snprintf(check, sizeof(check), "%s%s/generation", tbt_sysfs_path, router);
	if (is_link_nabs(check))
		exit(1);

	snprintf(gen, sizeof(gen), "cat %s%s/generation", tbt_sysfs_path, router);

	gen_str = do_bash_cmd(gen);
	generation = strtoud(gen_str);

	switch(generation) {
	case 1:
		printf("(TBT1)\n");
		break;
	case 2:
		printf("(TBT2)\n");
		break;
	case 3:
		printf("(TBT3)\n");
		break;
	case 4:
		printf("(USB4)\n");
		break;
	default:
		printf("(Unknown)\n");
	}

	free(gen_str);
}

/* Dump the NVM version of the router */
void dump_nvm_version(const char *router)
{
	char path[MAX_LEN], check[MAX_LEN];
	char *nvm;

	snprintf(check, sizeof(check), "%s%s/nvm_version", tbt_sysfs_path, router);
	if (is_link_nabs(check))
		exit(1);

	snprintf(path, sizeof(path), "cat %s%s/nvm_version", tbt_sysfs_path, router);
	nvm = do_bash_cmd(path);

	printf("NVM %s, ", nvm);

	free(nvm);
}

/* Dump the lanes used by the router at once */
void dump_lanes(const char *router)
{
	char path[MAX_LEN], check[MAX_LEN];
	char str[MAX_LEN];
	char *lanes;

	if (is_host_router(router)) {
		sprintf(str, "%s", "");
		return;
	}

	snprintf(check, sizeof(check), "%s%s/tx_lanes", tbt_sysfs_path, router);
	if (is_link_nabs(check))
		exit(1);

	snprintf(path, sizeof(path), "cat %s%s/tx_lanes", tbt_sysfs_path, router);
	lanes = do_bash_cmd(path);
	printf("x%s", lanes);

	free(lanes);
}

/* Dump the router's speed per lane */
void dump_speed(const char *router)
{
	char path[MAX_LEN], check[MAX_LEN];
	char str[MAX_LEN];
	char *speed_str;

	if (is_host_router(router)) {
		sprintf(str, "%s", "");
		return;
	}

	snprintf(check, sizeof(check), "%s%s/tx_speed", tbt_sysfs_path, router);
	if (is_link_nabs(check))
		exit(1);

	snprintf(path, sizeof(path), "cat %s%s/tx_speed", tbt_sysfs_path, router);

	speed_str = do_bash_cmd(path);
	printf("%s, ", speed_str);

	free(speed_str);
}

/* Dump the authentication status, depicting PCIe tunneling */
void dump_auth_sts(const char *router)
{
	char path[MAX_LEN], check[MAX_LEN];
	char *auth_str;
	bool auth;

	snprintf(check, sizeof(check), "%s%s/authorized", tbt_sysfs_path, router);
	if (is_link_nabs(check))
		exit(1);

	snprintf(path, sizeof(path), "cat %s%s/authorized", tbt_sysfs_path, router);

	auth_str = do_bash_cmd(path);
	auth = strtoud(auth_str);
	printf("Auth:%s\n", (auth == 1) ? "Yes" : "No");

	free(auth_str);
}

/* Returns the depth of the given valid router string */
u8 depth_of_router(const char *router)
{
	if (is_host_router(router))
		return 0;

	return (strlen(router) - 1) / 2;
}

/* Returns the domain of the valid router string */
u8 domain_of_router(const char *router)
{
	char *str = get_substr(router, 0, 1);
	u8 domain = strtoud(str);

	free(str);
	return domain;
}

/*
 * Returns the register value of the router config. space of the provided router
 * with the provided CAP_ID and VCAP_ID at the provided block offset.
 * Return a value of (u64)~0 if something goes wrong.
 *
 * Caller needs to ensure that the arguments are valid.
 */
u64 get_router_register_val(const char *router, u8 cap_id, u8 vcap_id, u64 off)
{
	struct router_config *config;
	char **regs = NULL;

	config = get_router_config_item(router);
	if (!config)
		return COMPLEMENT_BIT64;

	if (cap_id == 0x0)
		regs = config->regs;
	else if (cap_id == ROUTER_VCAP_ID && vcap_id == ROUTER_VSEC1_ID)
		regs = config->vsec1_regs;
	else if (cap_id == ROUTER_VCAP_ID && vcap_id == ROUTER_VSEC3_ID)
		regs = config->vsec3_regs;
	else if (cap_id == ROUTER_VCAP_ID && vcap_id == ROUTER_VSEC4_ID)
		regs = config->vsec4_regs;
	else if (cap_id == ROUTER_VCAP_ID && vcap_id == ROUTER_VSEC6_ID)
		regs = config->vsec6_regs;

	return get_register_val(regs, off);
}

/*
 * Returns the register value of the adapter config. space of the provided adapter
 * in the provided router with the given CAP_ID and SEC_ID at the given block
 * offset.
 * Return a value of (u64)~0 if something goes wrong.
 *
 * Caller needs to ensure that the arguments are valid.
 */
u64 get_adapter_register_val(const char *router, u8 cap_id, u8 sec_id, u8 adp, u64 off)
{
	struct router_config *router_config;
	struct adp_config *adp_config;
	char **regs = NULL;

	router_config = get_router_config_item(router);
	if (!router_config)
		return COMPLEMENT_BIT64;

	adp_config = get_adp_config_item(router, router_config->adps_config, adp);
	if (!adp_config)
		return COMPLEMENT_BIT64;

	if (cap_id == 0x0)
		regs = adp_config->regs;
	else if (cap_id == LANE_ADP_CAP_ID)
		regs = adp_config->lane_regs;
	else if (cap_id == USB4_PORT_CAP_ID)
		regs = adp_config->usb4_port_regs;
	else if (cap_id == USB3_ADP_CAP_ID && sec_id == USB3_ADP_SEC_ID)
		regs = adp_config->usb3_regs;
	else if (cap_id == PCIE_ADP_CAP_ID && sec_id == PCIE_ADP_SEC_ID)
		regs = adp_config->pcie_regs;
	else if (cap_id == DP_ADP_CAP_ID && sec_id == DP_ADP_SEC_ID)
		regs = adp_config->dp_regs;

	return get_register_val(regs, off);
}

/*
 * Returns 'true' if the arguments provided to the library are valid,
 * 'false' otherwise.
 */
bool is_arg_valid(const char *arg)
{
	u8 i = 0;

	if (!arg || strlen(arg) != 2)
		return false;

	if (arg[0] != '-')
		return false;

	for (; i < strlen(options); i++) {
		if (options[i] == arg[1])
			return true;
	}

	return false;
}

/* Actual 'main' function to distribute the functionalities */
int __main(char *domain, char *depth, char *device, bool retimer, bool tree,
	   u8 verbose)
{
	int ret;

	if (tree) {
		if (retimer) {
			fprintf(stderr, "invalid argument(s)\n%s", help_msg);
			return 1;
		} else
			return lstbt_t(domain, depth, device, verbose);
	} else if (retimer) {
		return lstbt_r(domain, depth, device);
	} else if (!tree && !verbose) {
		return lstbt(domain, depth, device);
	} else {
		ret = debugfs_config_init();
		if (ret)
			return ret;

		ret = lstbt_v(domain, depth, device, verbose);

		debugfs_config_exit();

		return ret;
	}

	return 0;
}

/* Split multiple argument strings into single ones */
char** ameliorate_args(int argc, char **argv)
{
	char **arr;
	int i, j;
	size_t k;

	arr = malloc(MAX_LEN * sizeof(char*));
	j = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strlen(argv[i]) == 1) {
				arr[j] = malloc(2 * sizeof(char));
				memset(arr[j], '\0', 2 * sizeof(char));

				arr[j++][0] = argv[i][0];

				continue;
			}

			for (k = 1; k < strlen(argv[i]); k++) {
				char str[3];

				snprintf(str, sizeof(str), "-%c", argv[i][k]);

				arr[j] = malloc(3 * sizeof(char));
				memset(arr[j], '\0', 3 * sizeof(char));
				strcpy(arr[j++], str);
			}
		} else {
			arr[j] = malloc((strlen(argv[i]) + 1) * sizeof(char));
			memset(arr[j], '\0', (strlen(argv[i]) + 1) * sizeof(char));

			memcpy(arr[j++], argv[i], strlen(argv[i]) * sizeof(char));
		}
	}

	arr[j] = NULL;
	return arr;
}

/* Returns 'true' if the input supplied is printable, 'false' otherwise */
bool is_input_printable(int argc, char **argv)
{
	int i = 0, j;

	for (; i < argc; i++) {
		for (j = 0; j < (int)strlen(argv[i]); j++) {
			if (!isprint(argv[i][j]))
				return false;
		}
	}

	return true;
}
