#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "helpers.h"

/*
 * No. of depths in a domain are constrained due to the limitation in route string
 * storage in the control packets.
 */
#define MAX_DEPTH_POSSIBLE	7

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

/* Validate the arguments for 'lstbt' and 'lstbt -t'  */
bool validate_args(const char *domain, const char *depth, const char *device)
{
	u8 domains = total_domains();
	u8 i = 0;

	if (device) {
		if (domain || depth)
			return false;
		else {
			for (; i < domains; i++) {
				if (!is_router_format(device, i))
					return false;
			}

			return true;
		}
	}

	if (domain) {
		if (strtoud(domain) >= domains)
			return false;
	}

	if (depth) {
		if (strtoud(depth) >= MAX_DEPTH_POSSIBLE)
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

/* Returns the depth of the given valid router string */
u8 depth_of_router(const char *router)
{
	if (is_host_router(router))
		return 0;

	return (strlen(router) - 1) / 2;
}
