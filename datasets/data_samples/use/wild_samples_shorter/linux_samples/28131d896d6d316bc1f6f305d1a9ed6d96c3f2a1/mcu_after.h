	MCU_S2D_H2CN
};

enum {
	MCU_FW_LOG_WM,
	MCU_FW_LOG_WA,
	MCU_FW_LOG_TO_HOST,
};

#define __MCU_CMD_FIELD_ID	GENMASK(7, 0)
#define __MCU_CMD_FIELD_EXT_ID	GENMASK(15, 8)
#define __MCU_CMD_FIELD_QUERY	BIT(16)
};

enum {
	MCU_WA_PARAM_PDMA_RX = 0x04,
	MCU_WA_PARAM_CPU_UTIL = 0x0b,
	MCU_WA_PARAM_RED = 0x0e,
};

#define MCU_CMD(_t)		FIELD_PREP(__MCU_CMD_FIELD_ID, MCU_CMD_##_t)
	struct sec_key key[2];
} __packed;

struct sta_phy {
	u8 type;
	u8 flag;
	u8 stbc;
	u8 sgi;

	__le32 sta_cap;

	struct sta_phy phy;
} __packed;

struct sta_rec_ra_fixed {
	__le16 tag;
	u8 op_vht_rx_nss;
	u8 op_vht_rx_nss_type;

	struct sta_phy phy;

	u8 spe_en;
	u8 short_preamble;
	u8 is_5g;
	u8 mmps_mode;
} __packed;

enum {
	RATE_PARAM_FIXED = 3,
	RATE_PARAM_FIXED_HE_LTF = 7,
	RATE_PARAM_FIXED_MCS,
	RATE_PARAM_FIXED_GI = 11,
	RATE_PARAM_AUTO = 20,
};

#define RATE_CFG_MCS			GENMASK(3, 0)
#define RATE_CFG_NSS			GENMASK(7, 4)
#define RATE_CFG_GI			GENMASK(11, 8)
#define RATE_CFG_BW			GENMASK(15, 12)