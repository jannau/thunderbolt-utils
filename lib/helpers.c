#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "helpers.h"

/*
 * No. of depths in a domain are constrained due to the limitation in route string
 * storage in the control packets.
 */
#define MAX_DEPTH_POSSIBLE	8

static char *tbt_debugfs_path = "/sys/kernel/debug/thunderbolt/";

static struct router_config *routers_config = NULL;

static char options[] = {'D', 'd', 's', 'r', 't', 'v', 'V', 'h'};

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

	snprintf(path, sizeof(path), "ls %s%s | grep 'port'", tbt_debugfs_path,
		 router);
	root_cmd = switch_cmd_to_root(path);

	item = do_bash_cmd_list(root_cmd);

	while (port--) {
		char port_val[MAX_LEN];

		snprintf(port_val, sizeof(port_val), "port%u", port);

		if (is_present_in_list(item, port_val))
			return port + 1;
	}

	return 0;
}

static struct list_item* get_cap_vcap_start(struct list_item *regs_list,
					    u8 cap_id, u8 vcap_id)
{
	u64 num_commas, col;
	bool found = false;
	s64 first, second;
	u64 total_col;
	char *regs;

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

				cap = strtouh(get_substr(regs, col, first - col));
				vcap = strtouh(get_substr(regs, first + 1,
							  second - first - 1));
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
	char path[MAX_LEN];
	char *root_cmd;

	snprintf(path, sizeof(path), "cat %s%s/regs | awk -v OFS=',' '{\\$1=\\$1}1'",
		 tbt_debugfs_path, router);
	root_cmd = switch_cmd_to_root(path);

	router_regs = do_bash_cmd_list(root_cmd);
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
	u8 i = 1;

	total_adps = get_total_adps_debugfs(router);

	for (; i < total_adps; i++) {
		char path[MAX_LEN];
		char *root_cmd;

		config[i].adp = i;

		snprintf(path, sizeof(path),
			 "cat 2>/dev/null %s%s/port%u/regs | awk -v OFS=',' '{\\$1=\\$1}1'",
			 tbt_debugfs_path, router, i);
		root_cmd = switch_cmd_to_root(path);

		adp_regs = do_bash_cmd_list(root_cmd);
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
	}
}

static struct router_config* get_router_config_item(struct router_config *configs,
						    const char *router)
{
	u64 i = 0;

	for (; ; i++) {
		if (!strcmp(configs[i].router, router))
			return &configs[i];
	}

	return NULL;
}

static struct adp_config* get_adp_config_item(struct adp_config *configs,
					      u8 adp)
{
	u64 i = 0;

	for (; ; i++) {
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

/* Initialize the debugfs parameters for faster access */
static void debugfs_config_init(void)
{
	struct list_item *router_list;
	char *root_cmd, *router;
	u64 total_routers, i;
	char path[MAX_LEN];
	u8 adps;

	snprintf(path, sizeof(path), "ls %s", tbt_debugfs_path);
	root_cmd = switch_cmd_to_root(path);

	router_list = do_bash_cmd_list(root_cmd);

	total_routers = get_total_list_items(router_list);
	routers_config = malloc(total_routers * sizeof(struct router_config));

	i = 0;

	for(; router_list; router_list = router_list->next) {
		router = (char*)router_list->val;
		adps = get_total_adps_debugfs(router);

		routers_config[i].router = router;
		get_router_regs(router, &routers_config[i]);

		routers_config[i].adps_config = malloc(adps *
						       sizeof(struct adp_config));
		get_adps_config(router, routers_config[i].adps_config);

		i++;
	}
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
	if (strtoud(output))
		return true;

	return false;
}

/* Returns the total no. of domains in the host */
u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_bash_cmd(path);

	return strtoud(output);
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
	if (strtoud(bash))
		return false;

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
	int pos;

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
	char vid_path[MAX_LEN], did_path[MAX_LEN];
	char *vid, *did;

	snprintf(vid_path, sizeof(vid_path), "cat %s%s/vendor", tbt_sysfs_path, router);
	vid = do_bash_cmd(vid_path);

	snprintf(did_path, sizeof(did_path), "cat %s%s/device", tbt_sysfs_path, router);
	did = do_bash_cmd(did_path);

	printf("ID %04x:%04x ", strtouh(vid), strtouh(did));
}

/* Dump the generation of the router */
void dump_generation(const char *router)
{
	char gen[MAX_LEN];
	u8 generation;

	snprintf(gen, sizeof(gen), "cat %s%s/generation", tbt_sysfs_path, router);
	generation = strtoud(do_bash_cmd(gen));

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
}

/* Dump the NVM version of the router */
void dump_nvm_version(const char *router)
{
	char path[MAX_LEN];
	char *nvm;

	snprintf(path, sizeof(path), "cat %s%s/nvm_version", tbt_sysfs_path, router);
	nvm = do_bash_cmd(path);

	printf("NVM %s, ", nvm);
}

/* Dump the router's lanes */
void dump_lanes(const char *router)
{
	char path[MAX_LEN];
	char *lanes;

	if (is_host_router(router)) {
		printf("");
		return;
	}

	snprintf(path, sizeof(path), "cat %s%s/tx_lanes", tbt_sysfs_path, router);
	lanes = do_bash_cmd(path);
	printf("x%s", lanes);
}

/* Dump the router's speed.
 * This doubles the 'tx_speed' represented in the sysfs.
 */
void dump_speed(const char *router)
{
	char path[MAX_LEN];
	u8 speed;

	if (is_host_router(router)) {
		printf("");
		return;
	}

	snprintf(path, sizeof(path), "cat %s%s/tx_speed", tbt_sysfs_path, router);
	speed = strtoud(do_bash_cmd(path));
	printf("%uG, ", speed * 2);
}

/* Dump the authentication status, depicting PCIe tunneling */
void dump_auth_sts(const char *router)
{
	char path[MAX_LEN];
	bool auth;

	snprintf(path, sizeof(path), "cat %s%s/authorized", tbt_sysfs_path, router);
	auth = strtoud(do_bash_cmd(path));
	printf("Auth:%s\n", (auth == 1) ? "Yes" : "No");
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
	return strtoud(get_substr(router, 0, 1));
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

	config = get_router_config_item(routers_config, router);
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
 * in the provided router with the given CAP_ID and VCAP_ID at the given block
 * offset.
 * Return a value of (u64)~0 if something goes wrong.
 *
 * Caller needs to ensure that the arguments are valid.
 */
u64 get_adapter_register_val(const char *router, u8 cap_id, u8 adp, u64 off)
{
	struct router_config *router_config;
	struct adp_config *adp_config;
	char **regs = NULL;

	router_config = get_router_config_item(routers_config, router);
	if (!router_config)
		return COMPLEMENT_BIT64;

	adp_config = get_adp_config_item(router_config->adps_config, adp);
	if (!adp_config)
		return COMPLEMENT_BIT64;

	if (cap_id == 0x0)
		regs = adp_config->regs;
	else if (cap_id == LANE_ADP_CAP_ID)
		regs = adp_config->lane_regs;
	else if (cap_id == USB4_PORT_CAP_ID)
		regs = adp_config->usb4_port_regs;
	else if (cap_id == USB3_ADP_CAP_ID)
		regs = adp_config->usb3_regs;
	else if (cap_id == PCIE_ADP_CAP_ID)
		regs = adp_config->pcie_regs;
	else if (cap_id == DP_ADP_CAP_ID)
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

/* Actual 'main' function to distribute the functions */
int __main(char *domain, char *depth, char *device, bool retimer, bool tree,
	   u8 verbose)
{
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
	} else if (verbose) {
		debugfs_config_init();
		return lstbt_v(domain, depth, device, verbose);
	}
}

/* Split multiple argument strings into single ones */
char** ameliorate_args(int argc, char **argv)
{
	int i, j, k;
	char **arr;

	arr = malloc(MAX_LEN * sizeof(char*));
	j = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strlen(argv[i]) == 1) {
				arr[j++] = argv[i];
				continue;
			}

			for (k = 1; k < strlen(argv[i]); k++) {
				char str[MAX_LEN];

				snprintf(str, sizeof(str), "-%c", argv[i][k]);

				arr[j] = malloc(2 * sizeof(char));
				strcpy(arr[j++], str);
			}
		} else
			arr[j++] = argv[i];
	}

	arr[j] = NULL;
	return arr;
}
