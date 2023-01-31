#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "passthrough.h"

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

u32 read_host_reg(u8 domain, u32 pos)
{
	return 0;
}

int main(void)
{
	char *pci_id = trim_host_pci_id(0);
	struct vdid *vdid = get_vdid(pci_id);
	printf("%s, %s\n", vdid->vendor_id, vdid->device_id);

	bind_grp_modules(pci_id, true);

	struct vfio_hlvl_params *params = vfio_dev_init(pci_id);
        printf("%d %d %d %d\n", params->container, params->group, params->device, params->dev_info->num_regions);
	get_dev_pci_cfg_regions(params, pci_id);

	struct list_item *temp = (params->pci_cfg_regions);
	for(;temp!=NULL;temp=temp->next) {
		struct vfio_region_info info = *(struct vfio_region_info*)(temp->val);
		printf("index:%d size:%x\n", info.index, info.size);
	}
}

