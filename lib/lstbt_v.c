static void dump_router_verbose(const char *router)
{
	char *route;
	bool exist;
	u8 domain;

	exist = is_router_present(router);
	if (!exist)
		return false;

	domain = strtoud(get_substr(router, 0, 1));
	route = get_route_string(get_top_id(router));

	printf("%s ", route);
}

static bool dump_domain_verbose(u8 domain, const u8 *depth, u8 num)
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
				found |= dump_router_verbose((char*)router->val, num);
		}
	} else {
		for(; router != NULL; router = router->next) {
			if (!is_router_format((char*)router->val, domain))
				continue;

			found |= dump_router_verbose((char*)router->val, num);
		}
	}

	return found;
}

/*
 * Function to be called with '-v' as the only additional argument.
 * @num: Indicates the number of 'v' provided as the argument (caps to 'vv').
 */
void lstbt_v(const u8 *domain, const u8 *depth, const char *device, u8 num)
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

		dump_router_verbose(device);
		return;
	}

	if (!domain && !depth) {
		i = 0;

		for (; i < domains; i++)
			found = dump_domain_verbose(i, NULL);
	} else if (domain && !depth) {
		found = dump_domain_verbose(strtoud(domain), NULL);
	} else if (!domain && depth) {
		i = 0;

		for (; i < domains; i++)
			found = dump_domain_verbose(i, depth);
	} else
		found = dump_domain_verbose(strtoud(domain), depth);

	if (!found)
		printf("no device(s) found\n");
}

int main(void)
{
	lstbt_v(NULL, NULL, NULL);
	return 0;
}
