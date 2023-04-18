#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "helpers.h"

/*
 * Check if the given string is in retimer string format.
 * Cases are limited for a thunderbolt directory.
 */
static bool is_retimer_format(const char *retimer, u8 domain)
{
	char num_str[MAX_LEN];
	char *colon_str = ":";
	int pos;

	snprintf(num_str, sizeof(num_str), "%u-", domain);

	pos = strpos(retimer, num_str, 0);
	if (pos)
		return false;

	pos = strpos(retimer, colon_str, 0);
	if (pos <= 0)
		return false;

	return true;
}

/*
 * Returns 'true' if the provided retimer is present in the given router,
 * 'false' otherwise.
 */
static bool is_retimer_in_router(const char *retimer, const char *router)
{
	char *rtr;
	int pos;

	pos = strpos(retimer, ":", 0);
	rtr = get_substr(retimer, 0, pos);

	if (strcmp(rtr, router))
		return false;

	return true;
}

/* Dumps the retimer f/w version */
static void dump_retimer_version(const char *retimer)
{
	char path[MAX_LEN];
	char *ver;

	snprintf(path, sizeof(path), "cat %s%s/nvm_version", tbt_sysfs_path,
		 retimer);

	ver = do_bash_cmd(path);
	printf("NVM %s\n", ver);
}

/* Dumps the retimer */
static bool dump_retimer(const char *retimer)
{
	char vid_path[MAX_LEN], did_path[MAX_LEN];
	int pos, dot_pos;
	char *vid, *did;
	u8 domain, port;
	char *router;

	domain = strtoud(get_substr(retimer, 0, 1));

	pos = strpos(retimer, ":", 0);
	router = get_substr(retimer, 0, pos);

	dot_pos = strpos(retimer, ".", pos);
	port = strtoud(get_substr(retimer, pos + 1, dot_pos - pos - 1));

	printf("Domain %u Router %u: Port %u: ", domain, router, port);

	snprintf(vid_path, sizeof(vid_path), "cat %s%s/vendor", tbt_sysfs_path, retimer);
	vid = do_bash_cmd(vid_path);

	snprintf(did_path, sizeof(did_path), "cat %s%s/device", tbt_sysfs_path, retimer);
	did = do_bash_cmd(did_path);

	printf("ID %04x:%04x ", strtouh(vid), strtouh(did));

	dump_nvm_version(retimer);

	return true;
}

/* Dumps the retimers (if any) present in the provided domain */
static bool enumerate_retimers_in_domain(u8 domain)
{
	struct list_item *retimer;
	char path[MAX_LEN];
	bool found = false;

	snprintf(path, sizeof(path), "for line in $(ls %s); do echo $line; done",
		 tbt_sysfs_path);

	retimer = do_bash_cmd_list(path);

	for (; retimer; retimer = retimer->next) {
		if (!is_retimer_format((char*)retimer->val, domain))
			continue;

		found |= dump_retimer((char*)retimer->val);
	}

	return found;
}

/* Dumps the retimers (if any) present on any port in the provided router */
static bool dump_retimers_in_router(const char *router)
{
	struct list_item *retimer;
	char path[MAX_LEN];
	bool found = false;
	u8 domain;

	domain = strtoud(get_substr(retimer, 0, 1));

	snprintf(path, sizeof(path), "for line in $(ls %s); do echo $line; done",
		 tbt_sysfs_path);

	retimer = do_bash_cmd_list(path);

	for (; retimer; retimer = retimer->next) {
		if (!is_retimer_format((char*)retimer->val, domain))
			continue;

		if (!is_retimer_in_router((char*)retimer->val, router))
			continue;

		found |= dump_retimer((char*)retimer->val);
	}

	return found;
}

static bool validate_args_r(const char *domain, const char *depth, const char *device)
{
	u8 domains = total_domains();
	u8 i = 0;

	if (depth)
		return false;

	if (device) {
		if (domain)
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

	return true;
}

/* Function to be called with '-r' as the extra argument */
void lstbt_r(const u8 *domain, const u8 *depth, const char *device)
{
	u8 domains = total_domains();
	bool found = false;
	u8 i;

	if (!domains) {
		fprintf(stderr, "thunderbolt can't be found\n");
		return;
	}

	if (!validate_args_r(domain, depth, device)) {
		fprintf(stderr, "invalid argument(s)\n");
		return;
	}

	if (device) {
		if (!is_router_present(device)) {
			fprintf(stderr, "invalid device\n");
			return;
		}

		found = dump_retimers_in_router(device);
	}

	if (!domain) {
		i = 0;

		for (; i < domains; i++)
			found = enumerate_retimers_in_domain(i);
	} else
		found = enumerate_retimers_in_domain(strtoud(domain));

	if (!found)
		printf("no retimer(s) found\n");
}

int main(void)
{
	lstbt_r(NULL, NULL, NULL);
	return 0;
}
