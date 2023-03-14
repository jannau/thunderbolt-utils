#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../utils.h"

/*
 * No. of depths in a domain are constrained due to the limitation in route string
 * storage in the control packets.
 */
#define MAX_DEPTH_POSSIBLE	7

#define APPEND_HEX_CHAR		2

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

/* Returns the total no. of domains in the host */
static u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_bash_cmd(path);

	return strtoud(output);
}

/*
 * Check if a given string is in router format.
 * Cases are limited for a thunderbolt directory.
 */
static bool is_router_format(const char *router, u8 domain)
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

/*
 * Returns the expected router string length in the given depth.
 * Caller needs to ensure the router string is in correct format.
 */
static inline u8 router_len_in_depth(u8 depth)
{
	if (!depth)
		return 3;

	return ((2 * depth) + 1);
}

static inline is_host_router(const char *router)
{
	return router[strlen(router)-1] == '0';
}

/* Returns the depth of the given valid router string */
static inline u8 depth_of_router(const char *router)
{
	if (is_host_router(router))
		return 0;

	return (strlen(router) - 1) / 2;
}

/*
 * Returns 'true' if the router is in the given depth.
 * Caller needs to ensure the router string is in correct format.
 */
static inline bool is_router_depth(const char *router, u8 depth)
{
	if (!depth)
		return is_host_router(router);

	return !is_host_router(router) && (strlen(router) == router_len_in_depth(depth));
}

static bool validate_args(const char *domain, const char *depth, const char *device)
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
static bool is_router_present(const char *router)
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

/* Dump the router's vendor/device IDs */
static void dump_vdid(const char *router)
{
	char vid_path[MAX_LEN], did_path[MAX_LEN];
	char *vid, *did;

	snprintf(vid_path, sizeof(vid_path), "cat %s%s/vendor", tbt_sysfs_path, router);
	vid = do_bash_cmd(vid_path);

	snprintf(did_path, sizeof(did_path), "cat %s%s/device", tbt_sysfs_path, router);
	did = do_bash_cmd(did_path);

	printf("ID %04x:%04x ", strtouh(vid), strtouh(did));
}

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

/* Dump the generation of the router */
static void dump_generation(const char *router)
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

static bool dump_router(const char *router)
{
	u8 domain, depth;
	bool exist;

	exist = is_router_present(router);
	if (!exist)
		return false;

	domain = strtoud(&router[0]);
	depth = depth_of_router(router);

	printf("Domain %u Depth %u: ", domain, depth);

	dump_vdid(router);
	dump_name(router);
	dump_generation(router);

	return true;
}

/*
 * Enumerate the domain.
 *
 * @depth: If NULL, enumerate the complete domain, else, enumerate the routers
 * in the provided depth belonging to the provided domain.
 *
 * Return 'true' if a device gets enumerated, 'false' otherwise.
 */
static bool enumerate_domain(u8 domain, const u8 *depth)
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
				found |= dump_router((char*)router->val);
		}
	} else {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			found |= dump_router((char*)router->val);
		}
	}

	return found;
}

/* Function to be called with singular 'lstbt' (no retimers/extra arguments) */
void lstbt(const u8 *domain, const u8 *depth, const char *device)
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

		dump_router(device);
		return;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found = enumerate_domain(i, NULL);
	} else if (domain && !depth) {
		found = enumerate_domain(strtoud(domain), NULL);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found = enumerate_domain(i, depth);
	} else
		found = enumerate_domain(strtoud(domain), depth);

	if (!found)
		printf("no devices found\n");
}

int main(void)
{
	lstbt(NULL, NULL, NULL);
	return 0;
}
