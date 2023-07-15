// SPDX-License-Identifier: GPL-2.0
/*
 * User-space utility for the thunderbolt/USB4 subsystem
 *
 * This file provides the output of the TBT/USB4 subsystem hierarchy in a
 * tree format.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "helpers.h"

#define VERBOSE_SPACES	4

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
	char *str = get_substr(router, 2, 1);
	u8 port = strtoud(str);

	free(str);
	return port;
}

/* Dump the vendor/device name of the router */
static void dump_name(const char *router)
{
	char v_path[MAX_LEN], d_path[MAX_LEN], vcheck[MAX_LEN], dcheck[MAX_LEN];
	char *vendor, *device;

	snprintf(vcheck, sizeof(vcheck), "%s%s/vendor_name", tbt_sysfs_path, router);
	snprintf(dcheck, sizeof(dcheck), "%s%s/device_name", tbt_sysfs_path, router);
	if (is_link_nabs(vcheck) || is_link_nabs(dcheck))
		exit(1);

	snprintf(v_path, sizeof(v_path), "cat %s%s/vendor_name", tbt_sysfs_path, router);
	vendor = do_bash_cmd(v_path);

	snprintf(d_path, sizeof(d_path), "cat %s%s/device_name", tbt_sysfs_path, router);
	device = do_bash_cmd(d_path);

	printf("%s %s ", vendor, device);

	free(vendor);
	free(device);
}

/*
 * Dump the router for 'lstbt -t' operation.
 *
 * @depth: Depth is not the router's depth, rather, the depth in the topology,
 * which will be printed on the console.
 * @verbose: 'True' if verbose output is needed, 'false' otherwise.
 */
static void dump_router(const char *router, u8 depth, bool verbose)
{
	u8 i = 0;

	dump_init_depth(depth);

	if (!depth)
		printf("Domain %u Depth %u: ", domain_of_router(router),
			depth_of_router(router));

	else
		printf("Port %u: ", downstream_port(router));

	dump_vdid(router);
	dump_name(router);

	dump_generation(router);

	if (!verbose)
		return;

	for (; i < VERBOSE_SPACES + 4 * depth; i++)
		printf(" ");

	if (!is_host_router(router)) {
		dump_nvm_version(router);

		dump_lanes(router);
		printf("/");
		dump_speed(router);
	}

	dump_auth_sts(router);
}

/* Enumerate the router provided and all the routers connected to its
 * downstream ports.
 *
 * @depth: Depth is the row number of the router enumeration in the output console.
 * @verbose: 'True' if verbose output is needed, 'false' otherwise.
 */
static bool enumerate_dev_tree(const char *router, u8 depth, bool verbose)
{
	u8 domain = domain_of_router(router);
	struct list_item *item, *head;
	char path[MAX_LEN];
	bool found = false;

	dump_router(router, depth, verbose);

	snprintf(path, sizeof(path), "for line in $(ls %s%s); do echo $line; done",
		 tbt_sysfs_path, router);

	item = do_bash_cmd_list(path);
	head = item;

	for (; item != NULL; item = item->next) {
		if (!is_router_format((char*)item->val, domain))
			continue;

		found |= enumerate_dev_tree((char*)item->val, depth + 1, verbose);
	}

	free_list(head);

	return true;
}

/* Enumerate the provided domain.
 *
 * @depth: If NULL, enumerate the complete domain, else, enumerate the routers
 * belonging to the provided depth in the given domain.
 * @verbose: 'True' if verbose output is needed, 'false' otherwise.
 */
static bool enumerate_domain_tree(u8 domain, char *depth, bool verbose)
{
	struct list_item *router, *head;
	char path[MAX_LEN];
	bool found = false;

	snprintf(path, sizeof(path), "for line in $(ls %s); do echo $line; done",
		 tbt_sysfs_path);

	router = do_bash_cmd_list(path);
	head = router;

	if (depth) {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			if (is_router_depth((char*)router->val, strtoud(depth)))
				found |= enumerate_dev_tree((char*)router->val, 0,
							    verbose);
		}
	} else {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			if (is_host_router((char*)router->val) &&
			    is_router_domain((char*)router->val, domain)) {
				found |= enumerate_dev_tree((char*)router->val, 0,
							    verbose);
				break;
			}
		}
	}

	free_list(head);

	return found;
}

/* Function to be called with '-t' and '-v' as the additional arguments.
 *
 * @verbose: 'True' if verbose output is needed, 'false' otherwise.
 */
int lstbt_t(char *domain, char *depth, char *device, bool verbose)
{
	u8 domains = total_domains();
	bool found = false;
	u8 i;

	if (!domains) {
		fprintf(stderr, "thunderbolt can't be found\n");
		return 1;
	}

	if (!validate_args(domain, depth, device)) {
		fprintf(stderr, "invalid argument(s)\n%s", help_msg);
		return 1;
	}

	if (device) {
		if (!is_router_present(device)) {
			fprintf(stderr, "invalid device\n");
			return 1;
		}

		enumerate_dev_tree(device, 0, verbose);
		return 0;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found |= enumerate_domain_tree(i, NULL, verbose);
	} else if (domain && !depth) {
		found = enumerate_domain_tree(strtoud(domain), NULL, verbose);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found |= enumerate_domain_tree(i, depth, verbose);
	} else
		found = enumerate_domain_tree(strtoud(domain), depth, verbose);

	if (!found)
		printf("no device(s) found\n");

	return 0;
}
