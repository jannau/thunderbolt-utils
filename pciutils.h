//#include <stdbool.h>

//#include "pciutils/lib/pci.h"
#include "utils.h"

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
struct list_item* get_bars_offset(const char *pci_id);

/*struct pci_access* init_pci(void);
void clean_pci(struct pci_access *pacc);
bool find_host_controller(void);*/
