// SPDX-License-Identifier: GPL-2.0
/*
 * User-space utility for the thunderbolt/USB4 subsystem
 *
 * This file encompasses the functionalities of singular lstbt (i.e.,
 * without any extra arguments apart from domain, device, or depth).
 * Further, this is the main file to fetch the arguments from the
 * command line.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "helpers.h"

#define LIBTBT_MAJ_VERSION	0
#define LIBTBT_MIN_VERSION	1

#define APPEND_HEX_CHAR		2

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

static bool dump_router(const char *router)
{
	u8 domain, depth;
	bool exist;
	char *str;

	exist = is_router_present(router);
	if (!exist)
		return false;

	str = get_substr(router, 0, 1);
	domain = strtoud(str);

	depth = depth_of_router(router);

	printf("Domain %u Depth %u: ", domain, depth);

	dump_vdid(router);
	dump_name(router);
	dump_generation(router);

	free(str);

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
static bool enumerate_domain(u8 domain, char *depth)
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
				found |= dump_router((char*)router->val);
		}
	} else {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			found |= dump_router((char*)router->val);
		}
	}

	free_list(head);

	return found;
}

/* Function to be called with singular 'lstbt' (no retimers/extra arguments) */
int lstbt(char *domain, char *depth, char *device)
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

		dump_router(device);
		return 0;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found |= enumerate_domain(i, NULL);
	} else if (domain && !depth) {
		found = enumerate_domain(strtoud(domain), NULL);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found |= enumerate_domain(i, depth);
	} else
		found = enumerate_domain(strtoud(domain), depth);

	if (!found)
		printf("no device(s) found\n");

	return 0;
}

int main(int argc, char **argv)
{
	char *domain, *depth, *device, *prev;
	bool tree, retimer;
	u8 verbose = 0;
	int ret = 0;
	char **arr;
	u32 i = 0;

	if (!is_input_printable(argc, argv)) {
		fprintf(stderr, "discovered non-printable character(s), exiting...\n");
		exit(1);
	}

	domain = depth = device = prev = NULL;
	tree = retimer = false;

	arr = ameliorate_args(argc, argv);

	for (; i < MAX_LEN; i++) {
		if (arr[i] == NULL)
			break;

		if (prev) {
			if (prev[1] == 'D' && !domain)
				domain = arr[i];
			else if (prev[1] == 'd' && !depth)
				depth = arr[i];
			else if (prev[1] == 's' && !device)
				device = arr[i];

			prev = NULL;
			continue;
		}

		if (!is_arg_valid(arr[i])) {
			fprintf(stderr, "lstbt: invalid option -- '%s'\n", arr[i]);
			fprintf(stderr, "%s", help_msg);

			ret = 1;
			goto out;		}

		if (arr[i][1] == 'D' || arr[i][1] == 'd' || arr[i][1] == 's')
			prev = arr[i];
		else if (arr[i][1] == 'r')
			retimer = true;
		else if (arr[i][1] == 't')
			tree = true;
		else if (arr[i][1] == 'v')
			verbose++;
		else if (arr[i][1] == 'h') {
			printf("%s", help_msg);

			ret = 0;
			goto out;
		}
		else if (arr[i][1] == 'V') {
			printf("lstbt (thunderbolt-utils) %u.%u\n", LIBTBT_MAJ_VERSION,
			       LIBTBT_MIN_VERSION);

			ret = 0;
			goto out;
		}
	}

	/* If 'prev' is not 'NULL', it implies that an argument is missing */
	if (prev) {
		fprintf(stderr, "missing argument(s)\n%s", help_msg);

		ret = 1;
		goto out;
	}

	ret = __main(domain, depth, device, retimer, tree, verbose);

out:
	for (i = 0; i < MAX_LEN * MAX_LEN; i++) {
		if (!arr[i])
			break;

		free(arr[i]);
	}
	free(arr);

	return ret;
}
