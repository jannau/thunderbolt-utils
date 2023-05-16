#include <stdio.h>

#include "tbtutils.h"

int main(void)
{
	/* Fetch the PCI ID for the host thunderbolt controller for domain 0 */
	char *pci_id = trim_host_pci_id(0);
	struct vfio_hlvl_params *params;
	char **dev_list;
	u64 num;
	int ret;

	if (!pci_id)
		return 1;

	/*
	 * Fetch the count of the no. of modules in the same IOMMU group as the
	 * PCI device.
	 */
	num = total_grp_modules(pci_id);

	/* Check the presence of VFIO module in the system */
	if (!check_vfio_module()) {
		fprintf(stderr, "VFIO not found\n");
		return 1;
	}

	/* Bind all the modules present in the same IOMMU group as the PCI device */
	dev_list = bind_grp_modules(pci_id);

	/* Initialize the VFIO for the PCI device */
	params = vfio_dev_init(pci_id);
	if (!params)
		return 1;

	/* Fetch the BAR and PCI config. space regions for the PCI device */
	get_dev_bar_regions(params);
	get_dev_pci_cfg_region(params);

	/* Reset the host interface registers for the thunderbolt host controller */
	reset_host_interface(params);

	/* Initialize the thunderbolt hardware before executing anything */
	ret = tbt_hw_init(pci_id);
	if (ret)
		return ret;

	/* Allocate the TX and RX descriptors for the host thunderbolt controller */
	allocate_tx_desc(params);
	allocate_rx_desc(params);

	/* Initialize the TX and RX host interface registers for the thunderbolt controller */
	init_host_tx(params);
	init_host_rx(params);

	/* Request 1 dword from router config. space at offset 0x0 */
	ret = request_router_cfg(pci_id, params, 0, 0, 1);

	unbind_grp_modules(dev_list, num);

	return ret;
}
