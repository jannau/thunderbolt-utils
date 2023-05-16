// SPDX-License-Identifier: GPL-3.0

// Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>

#include "tb_cfg.h"

char* trim_host_pci_id(u8 domain);
void reset_host_interface(const struct vfio_hlvl_params *params);
void allocate_tx_desc(const struct vfio_hlvl_params *params);
void allocate_rx_desc(const struct vfio_hlvl_params *params);
void init_host_tx(const struct vfio_hlvl_params *params);
void init_host_rx(const struct vfio_hlvl_params *params);
int request_router_cfg(const char *pci_id, const struct vfio_hlvl_params *params,
		       u64 route, u32 addr, u64 dwords);
int tbt_hw_init(const char *pci_id);

