#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "pciutils.h"

#define TRIM_NUM_PATH	13

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

static u8 total_domains(void)
{
	char path[MAX_LEN];
	char *output;

	snprintf(path, sizeof(path), "ls 2>/dev/null %s | grep domain | wc -l",
		 tbt_sysfs_path);

	output = do_bash_cmd(path);

	return strtoul(output, &output, 10);
}

static char* trim_host_pci_id(const u8 domain)
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
	pci_id[TRIM_NUM_PATH - 1] = '\0';

	return pci_id;
}

int main(void)
{
	char *pci_id = trim_host_pci_id(0);
	struct vdid *vdid = get_vdid(pci_id);
	printf("%s, %s\n", vdid->vendor_id, vdid->device_id);

	bind_grp_modules(pci_id, true);

	struct vfio_hlvl_params *params = vfio_dev_init(pci_id);
        printf("%d %d %d %d\n", params->container, params->group, params->device, params->dev_info->num_regions);
	get_dev_bar_regions(params, pci_id);
	get_dev_pci_cfg_region(params, pci_id);
	struct vfio_region_info *find_bar = find_bar_for_off(params->bar_regions, 0x0);
	printf("bar:%d\n", find_bar->index);
	printf("%d\n",get_page_aligned_addr(4096));

	printf("mem:0x%x\n", read_host_mem_byte(params, 0x39880));
	write_host_mem(params, 0x39880, 0x0);
	printf("mem:0x%x\n", read_host_mem_byte(params, 0x39880));
}
