//#include <stdbool.h>
//#include "pciutils/lib/pci.h"
#include "passthrough.h"

#define VDID_LEN		4
#define TRIM_VDID_PATH		10

#define INTEL_VID		"8086"

#define PCI_CMD			0x4
#define PCI_CMD_MEM		0x2
#define PCI_CMD_MASTER		0x4

static char *pci_drv_sysfs_path = "/sys/bus/pci/drivers/";
static char *pci_dev_sysfs_path = "/sys/bus/pci/devices/";

struct vdid {
	char vendor_id[MAX_LEN];
	char device_id[MAX_LEN];
};

void do_pci_rescan(void);
void remove_pci_dev(const char *pci_id);
struct vdid* get_vdid(const char *pci_id);
void allow_bus_master(const char *pci_id);
