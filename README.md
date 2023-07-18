# thunderbolt-utils
Thunderbolt/USB4 utilities

This software is a collection of various user-space functionalities for the thunderbolt/USB4 subsystem, along with a library (lstbt) to query the TBT/USB4 devices present in
the system.

Utilities included:
1. lstbt
2. Framework abstractions to provide the user wrappers to configure the TBT/USB4 subsystem via porting the DMA layer from kernel-space to user-space, paving way for independent software around it.

## Building and installing lstbt
lstbt is present in the ./lib subdirectory of the software.

Steps:<br>
1. `cd ./lib`
2. `make`
3. `make install`

## cleaning up the build

Steps:<br>
1. `cd ./lib`
2. `make clean`

Note: The library installs itself in the /usr/bin filesystem path, hence pertinent permissions are required for the user to alter it.
Note 2: It is not necessary to install lstbt for testing it.

## TBT/USB4 user-space functionalities
This software serves as the first prefatory abstraction of various functionalities of the TBT/USB4 subsystem a user can
utilize to perform a myriad of operations pertinent to the config. space access of the routers and setting up paths
to facilitate DMA transactions.

Thunderbolt/USB4 host routers (with PCIe host interface layer) inherently have host interface registers, which are made
accessible to the host via the PCI BARs.<br>
These registers incorporate various functionalities including, but not limited to:<br>
1. Control (reset/CLx)<br>
2. TX ring 0<br>
   - Base address (IO virtual address for IOMMU mapping)<br>
   - Ring size/control<br>
   - Producer/consumer indexes<br>
3. RX ring 0<br>
   - Base address (IO virtual address for IOMMU mapping)<br>
   - Ring size/control<br>
   - Producer/consumer indexes<br>
4. Interrupts

The host essentially needs to communicate with these registers to access the config. space and incorporate the framework
needed to facilitate DMA transactions.

Control path of the thunderbolt/USB4 subsystem is routed with the hop ID of 0, which the transport layer prepends in the
control packet received.<br>
The descriptors which house such control packets reside in TX ring-0 and RX ring-0, which the software uses.

To provide user better controllability of the TBT/USB4 subsystem, the DMA needs to be ported from kernel-space to the
user-space, which would then conspicuously provide the user with all the operations needed to regulate the subsystem
(in an IOMMU-protected environment). 

This software incorporates the DMA porting with the help of VFIO.<br>
For more details on VFIO, refer to the kernel documentation:<br>
https://docs.kernel.org/driver-api/vfio.html

In its rudimentary form, the software incorporates a VFIO-driven way to faciliate the DMA transmission. Please refer
'example.c' to understand more on how its been done.

Ameliorations are being made consistently for the software to abstract all the functionalities needed for the user to
drive the subsystem including interrupts, router notifications, and possibly the inter-domain packets sometime later.
