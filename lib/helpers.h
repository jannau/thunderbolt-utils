#include <stdbool.h>

#include "../utils.h"

static char *tbt_sysfs_path = "/sys/bus/thunderbolt/devices/";

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
