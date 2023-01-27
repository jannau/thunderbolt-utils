#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

#define TRIM_NUM_PATH	13

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

static u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_ssh_cmd(path);

	return strtoul(output, &output, 10);
}

char* trim_host_pci_id(u8 domain)
{
	char *pci_id = malloc(MAX_LEN * sizeof(char));
	char path[MAX_LEN];
	u16 pos;

	if (total_domains() < (domain + 1)) {
		printf("invalid domain\n");
		return NULL;
	}

	snprintf(path, sizeof(path), "%s%d-0", tbt_sysfs_path, domain);

	readlink(path, path, MAX_LEN);

	pos = strpos(path, "domain", 0);
	pos -= TRIM_NUM_PATH;

	strncpy(pci_id, path + pos, TRIM_NUM_PATH - 1);

	return pci_id;
}

u32 read_host_reg(u8 domain, u32 pos)
{
	return 0;
}

int main(void)
{
	char *s = trim_host_pci_id(0);

        printf("%s\n", s);
}
