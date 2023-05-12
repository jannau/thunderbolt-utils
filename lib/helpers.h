#include <stdbool.h>

#include "../utils.h"
#include "adapter.h"
#include "router.h"

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
	char **regs;
	char **lane_regs;
	char **pcie_regs;
	char **dp_regs;
	char **usb3_regs;
	char **usb4_port_regs;
};

static struct router_config {
	char *router;

	char **regs;
	char **vsec1_regs;
	char **vsec3_regs;
	char **vsec4_regs;
	char **vsec6_regs;

	struct adp_config *adps_config;
};
static struct router_config *routers_config;

static char options[] = {'D', 'd', 's', 'r', 't', 'v', 'V', 'h'};
static char *help_msg =
"Usage: lstbt [options]...\n"
"List TBT/USB4 devices\n"
"  -D domain\n"
"      Select the domain lstbt will examine\n"
"  -d depth\n"
"      Select the depth (starting from 0) lstbt will consider\n"
"  -s device\n"
"      Select the device (like displayed in sysfs) lstbt will examine\n"
"  -r retimer\n"
"      Display the retimers present in the thunderbolt subsystem\n"
"  -t tree\n"
"      Display the thunderbolt subsystem in tree format\n"
"  -v verbose\n"
"      Increase the verbosity (-vv for higher)\n"
"  -V version\n"
"      Display the version of the library\n"
"  -h help\n"
"      Display the usage\n";

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
u64 get_router_register_val(const char *router, u8 cap_id, u8 vcap_id, u64 off);
u64 get_adapter_register_val(const char *router, u8 adp, u8 cap_id,
			     u8 vcap_id, u64 off);
bool is_arg_valid(const char *arg);
int lstbt(const u8 *domain, const u8 *depth, const char *device);
int lstbt_t(const u8 *domain, const u8 *depth, const char *device, bool verbose);
int lstbt_v(const u8 *domain, const u8 *depth, const char *device, u8 num);
int lstbt_r(const u8 *domain, const u8 *depth, const char *device);
int __main(const char *domain, const char *depth, const char *device,
	   bool retimer, bool tree, u8 verbose);
char** ameliorate_args(int argc, char **argv);
