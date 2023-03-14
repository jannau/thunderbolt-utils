#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "helpers.h"

#define APPEND_HEX_CHAR		2

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

static bool dump_router(const char *router)
{
	u8 domain, depth;
	bool exist;

	exist = is_router_present(router);
	if (!exist)
		return false;

	domain = strtoud(get_substr(router, 0, 1));
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
		printf("no device(s) found\n");
}

int main(void)
{
	lstbt(NULL, NULL, NULL);
	return 0;
}
