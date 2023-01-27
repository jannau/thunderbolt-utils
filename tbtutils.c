#include <string.h>
#include <stdio.h>

#include "utils.h"

#define PATH_MAX_LEN	1024
#define TRIM_NUM_PATH	13

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

char* trim_host_pci_id(u8 domain)
{
	char path[PATH_MAX_LEN];
	char *pci_id;
	u16 pos;

	snprintf(path, sizeof(path), "%s%d-0", tbt_sysfs_path, domain);

	readlink(path, path, PATH_MAX_LEN);

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
	char *s = trim_host_pci_id(1);

        printf("%s\n", s);
}
