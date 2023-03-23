#include <stdbool.h>
#include <stdio.h>

#include "helpers.h"

/* Returns the domain of the valid router string */
static inline u8 domain_of_router(const char *router)
{
	return strtoud(get_substr(router, 0, 1));
}

/* Returns 'true' if the router is in the provided domain, 'false' otherwise */
static inline bool is_router_domain(const char *router, u8 domain)
{
	return domain_of_router(router) == domain;
}

/* Depth here refers to the depth in the topological output */
static inline u8 total_whitespace(u8 depth)
{
	if (!depth || (depth == 1))
		return 0;

	return 4 * (depth - 1);
}

/* Depth refers to the description in the above function */
static void dump_init_depth(u8 depth)
{
	u8 i = 1;

	if (!depth) {
		printf("/:  ");
		return;
	}

	printf("    ");

	for (; i <= total_whitespace(depth); i++)
		printf(" ");

	printf("|__ ");
}

/*
 * Returns the lane-0 adapter number of the port of the upstream router,
 * by which the provided valid router is connected.
 */
static inline u8 downstream_port(const char *router)
{
	return strtoud(get_substr(router, 2, 1));
}

/* Dump the router's lanes */
static void dump_lanes(const char *router)
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
static void dump_speed(const char *router)
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
static void dump_auth_sts(const char *router)
{
	char path[MAX_LEN];
	bool auth;

	snprintf(path, sizeof(path), "cat %s%s/authorized", tbt_sysfs_path, router);
	auth = strtoud(do_bash_cmd(path));
	printf("Auth:%s ", (auth == 1) ? "Yes" : "No");
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

	printf("%s %s, ", vendor, device);
}

/*
 * Dump the router for 'lstbt -t' operation.
 *
 * @depth: Depth is not the router's depth, rather, the depth in the topology,
 * which will be printed on the console.
 */
static void dump_router(const char *router, u8 depth)
{
	dump_init_depth(depth);

	if (!depth)
		printf("Domain %u Depth %u: ", domain_of_router(router),
			depth_of_router(router));

	else
		printf("Port %u: ", downstream_port(router));

	dump_vdid(router);

	dump_name(router);

	if (!is_host_router(router)) {
		dump_lanes(router);
		printf("/");
		dump_speed(router);
	}

	dump_auth_sts(router);

	dump_generation(router);
}

/* Enumerate the router provided and all the routers connected to its
 * downstream ports.
 *
 * @depth: Depth is the row number of the router enumeration in the output console.
 */
static bool enumerate_dev_tree(const char *router, u8 depth)
{
	u8 domain = domain_of_router(router);
	struct list_item *item;
	char path[MAX_LEN];
	bool found = false;

	dump_router(router, depth);

	snprintf(path, sizeof(path), "for line in $(ls %s%s); do echo $line; done",
		 tbt_sysfs_path, router);

	item = do_bash_cmd_list(path);

	for (; item != NULL; item = item->next) {
		if (!is_router_format((char*)item->val, domain))
			continue;

		found |= enumerate_dev_tree((char*)item->val, depth + 1);
	}

	return true;
}

/* Enumerate the provided domain.
 *
 * @depth: If NULL, enumerate the complete domain, else, enumerate the routers
 * belonging to the provided depth in the given domain.
 */
static bool enumerate_domain_tree(u8 domain, const u8 *depth)
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
				found |= enumerate_dev_tree((char*)router->val, 0);
		}
	} else {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			if (is_host_router((char*)router->val) &&
			    is_router_domain((char*)router->val, domain)) {
				found |= enumerate_dev_tree((char*)router->val, 0);
				break;
			}
		}
	}

	return found;
}

/* Function to be called with '-t' as the only additional argument */
void lstbt_t(const u8 *domain, const u8 *depth, const char *device)
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

		enumerate_dev_tree(device, 0);
		return;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found = enumerate_domain_tree(i, NULL);
	} else if (domain && !depth) {
		found = enumerate_domain_tree(strtoud(domain), NULL);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found = enumerate_domain_tree(i, depth);
	} else
		found = enumerate_domain_tree(strtoud(domain), depth);

	if (!found)
		printf("no device(s) found\n");
}

int main()
{
	lstbt_t(NULL, NULL, NULL);
	return 0;
}
