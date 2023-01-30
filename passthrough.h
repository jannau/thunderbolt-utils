#include <stdbool.h>

#include "utils.h"

struct vdid {
	char vendor_id[MAX_LEN];
	char device_id[MAX_LEN];
};

bool check_vfio_module(void);
struct vdid* get_vdid(const char *pci_id);
void bind_grp_modules(const char *pci_id, bool bind);
