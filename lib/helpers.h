// SPDX-License-Identifier: GPL-3.0

// Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>


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

extern char *tbt_sysfs_path;

struct adp_config {
	u8 adp;
	char **regs;
	char **lane_regs;
	char **pcie_regs;
	char **dp_regs;
	char **usb3_regs;
	char **usb4_port_regs;
};

struct router_config {
	char *router;

	char **regs;
	char **vsec1_regs;
	char **vsec3_regs;
	char **vsec4_regs;
	char **vsec6_regs;

	struct adp_config *adps_config;
};

extern char *help_msg;

bool is_adp_present(const char *router, u8 adp);
u8 total_domains(void);
bool validate_args(char *domain, char *depth, const char *device);
bool is_router_present(const char *router);
bool is_router_format(const char *router, u8 domain);
bool is_host_router(const char *router);
u8 router_len_in_depth(u8 depth);
bool is_router_depth(const char *router, u8 depth);
void dump_vdid(const char *router);
void dump_nvm_version(const char *router);
void dump_generation(const char *router);
void dump_lanes(const char *router);
void dump_speed(const char *router);
void dump_auth_sts(const char *router);
u8 depth_of_router(const char *router);
u8 domain_of_router(const char *router);
u64 get_router_register_val(const char *router, u8 cap_id, u8 vcap_id, u64 off);
u64 get_adapter_register_val(const char *router, u8 adp, u8 cap_id, u64 off);
bool is_arg_valid(const char *arg);
int lstbt(char *domain, char *depth, char *device);
int lstbt_t(char *domain, char *depth, char *device, bool verbose);
int lstbt_v(char *domain, char *depth, char *device, u8 num);
int lstbt_r(char *domain, const char *depth, char *device);
int __main(char *domain, char *depth, char *device, bool retimer, bool tree,
	   u8 verbose);
char** ameliorate_args(int argc, char **argv);
