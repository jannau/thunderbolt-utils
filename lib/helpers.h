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

static char *cfg_space_tab = "cat %s%s/regs | awk -v OFS=',' '{\\$1=\\$1}1'";
static char *cap_vcap_search = "awk -F',' '\\$3 ~ /0x%02x/' | awk -F',' '\\$4 ~ /0x%02x/'";
static char *print_row_num = "sed -n '%up'";
static char *print_col_in_row = "awk -F',' '{print \\$5}'";

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
