/* Basic adapter configuration space */
#define ADP_CS_2			0x2
#define ADP_CS_2_SUB_TYPE		BITMASK(7, 0)
#define ADP_CS_2_VERSION		BITMASK(15, 8)
#define ADP_CS_2_PROTOCOL		BITMASK(23, 16)

#define ADP_CS_3			0x3
#define ADP_CS_3_NUM			BITMASK(25, 20)
#define ADP_CS_3_PLUGGED		BIT(30)
#define ADP_CS_3_LOCK			BIT(31)

#define ADP_CS_5			0x5
#define ADP_CS_5_DHP			BIT(31)

/*
 * Protocol, version, and sub-type fields of the respective adapter.
 * This represents bits [23:0] of the common ADP_CS_2 register.
 */
#define UNSUPPORTED_PVS			0x000000
#define LANE_PVS			0x000001
#define HOST_INTERFACE_PVS		0x000002
#define DOWN_PCIE_PVS			0x100101
#define UP_PCIE_PVS			0x100102
#define DP_OUT_PVS			0x0e0102
#define DP_IN_PVS			0x0e0101
#define DOWN_USB3_PVS			0x200101
#define UP_USB3_PVS			0x200102

/* Lane adapter configuration space */
#define LANE_ADP_CS_0			0x0
#define LANE_ADP_CS_0_SUP_SPEEDS	BITMASK(19, 16) /* Same for Lane-1 as well */
#define LANE_ADP_CS_0_SUP_WIDTH		BITMASK(25, 20)
#define LANE_ADP_CS_0_CL0S_SUP		BIT(26)
#define LANE_ADP_CS_0_CL1_SUP		BIT(27)
#define LANE_ADP_CS_0_CL2_SUP		BIT(28)

#define LANE_ADP_CS_1			0x1
#define LANE_ADP_CS_1_CL0S_EN		BIT(10)
#define LANE_ADP_CS_1_CL1_EN		BIT(11)
#define LANE_ADP_CS_1_CL2_EN		BIT(12)
#define LANE_ADP_CS_1_LD		BIT(14)
#define LANE_ADP_CS_1_LB		BIT(15)
#define LANE_ADP_CS_1_CUR_LINK_SPEED	BITMASK(19, 16) /* Same for Lane-1 as well */
#define LANE_ADP_CS_1_NEG_LINK_WIDTH	BITMASK(25, 20) /* Same for Lane-1 as well */
#define LANE_ADP_CS_1_ADP_STATE		BITMASK(29, 26)
#define LANE_ADP_CS_1_PMS		BIT(30)

#define LANE_SPEED_GEN3			BIT(18)
#define LANE_SPEED_GEN2			BIT(19)
#define LANE_WIDTH_X1			BIT(20)
#define LANE_WIDTH_X2			BIT(21)

/* USB4 port configuration space (only for Lane-0 adapter) */
#define PORT_CS_18			0x12
#define PORT_CS_18_CUSB4_VER		BITMASK(7, 0)
#define PORT_CS_18_BE			BIT(8)
#define PORT_CS_18_TCM			BIT(9)
#define PORT_CS_18_CPS			BIT(10)
#define PORT_CS_18_WOCS			BIT(16)
#define PORT_CS_18_WODS			BIT(17)
#define PORT_CS_18_WOU4S		BIT(18)

#define PORT_CS_19			0x13
#define PORT_CS_19_DPR			BIT(0)
#define PORT_CS_19_PC			BIT(3)
#define PORT_CS_19_EWOC			BIT(16)
#define PORT_CS_19_EWOD			BIT(17)
#define PORT_CS_19_EWOU4		BIT(18)

/* USB3 configuration space */
#define ADP_USB3_CS_0			0x0
#define ADP_USB3_CS_0_VALID		BIT(30)
#define ADP_USB3_CS_0_PE		BIT(31)

#define ADP_USB3_CS_1			0x1
#define ADP_USB3_CS_1_CUB		BITMASK(11, 0)
#define ADP_USB3_CS_1_CDB		BITMASK(23, 12)
#define ADP_USB3_CS_1_HCA		BIT(31)

#define ADP_USB3_CS_2			0x2
#define ADP_USB3_CS_2_AUB		BITMASK(11, 0)
#define ADP_USB3_CS_2_ADB		BITMASK(23, 12)
#define ADP_USB3_CS_2_CMR		BIT(31)

#define ADP_USB3_CS_3			0x3
#define ADP_USB3_CS_3_SCALE		BITMASK(5, 0)

#define ADP_USB3_CS_4			0x4
#define ADP_USB3_CS_4_ALR		BITMASK(6, 0)
#define ADP_USB3_CS_4_ULV		BIT(7)
#define ADP_USB3_CS_4_PLS		BITMASK(11, 8)
#define ADP_USB3_CS_4_MAX_SUP_LR	BITMASK(18, 12)
