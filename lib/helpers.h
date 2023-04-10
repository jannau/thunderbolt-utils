#include <stdbool.h>

#include "../utils.h"

/* Maximum adapters possible in a router */
#define MAX_ADAPTERS		64

/* Protocol identifiers */
#define PROTOCOL_PCIE		0
#define PROTOCOL_USB3		1
#define PROTOCOL_DP		2
#define PROTOCOL_HCI		3

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";
static char *tbt_debugfs_path = "/sys/kernel/debug/thunderbolt/";

static struct adp_config {
	u8 adp;
	struct list_item *regs;
};

static struct router_config {
	char *router;
	struct list_item *regs;
	struct adp_config *adps_config;
};
static struct router_config *routers_config;

u8 total_domains(void);
bool validate_args(const char *domain, const char *depth, const char *device);
bool is_router_present(const char *router);
bool is_router_format(const char *router, u8 domain);
bool is_host_router(const char *router);
u8 router_len_in_depth(u8 depth);
bool is_router_depth(const char *router, u8 depth);
void dump_vdid(const char *router);
void dump_generation(const char *router);
u8 depth_of_router(const char *router);
u8 domain_of_router(const char *router);
void debugfs_config_init(void);
u64 get_router_register_val(const char *router, u8 cap_id, u8 vcap_id, u64 off);
u64 get_adapter_register_val(const char *router, u8 adp, u8 cap_id,
			     u8 vcap_id, u64 off);
