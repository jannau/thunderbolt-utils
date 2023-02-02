//#include <stdbool.h>
//#include "pciutils/lib/pci.h"
#include "passthrough.h"

#define VDID_LEN	4
#define TRIM_VDID_PATH	10

#define INTEL_VID	"8086"

static char *pci_drv_sysfs_path = "/sys/bus/pci/drivers/";
static char *pci_dev_sysfs_path = "/sys/bus/pci/devices/";

struct vdid {
	char vendor_id[MAX_LEN];
	char device_id[MAX_LEN];
};

void do_pci_rescan(void);
void remove_pci_dev(const char *pci_id);
struct vdid* get_vdid(const char *pci_id);
u32 read_pci_cfg_long(const struct vfio_hlvl_params *params, u64 off);
/*struct pci_access* init_pci(void);
void clean_pci(struct pci_access *pacc);
bool find_host_controller(void);*/
