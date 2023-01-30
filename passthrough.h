#include <linux/vfio.h>
#include <stdbool.h>

#include "utils.h"

struct vdid {
	char vendor_id[MAX_LEN];
	char device_id[MAX_LEN];
};

struct vfio_hlvl_params {
	int container;
	int group;
	int device;
	struct vfio_device_info *dev_info;
};

bool check_vfio_module(void);
struct vdid* get_vdid(const char *pci_id);
void bind_grp_modules(const char *pci_id, bool bind);
struct vfio_hlvl_params* vfio_dev_init(const char *pci_id);
