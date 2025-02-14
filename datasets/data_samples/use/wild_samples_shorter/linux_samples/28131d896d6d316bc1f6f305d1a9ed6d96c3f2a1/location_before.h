	IWL_TOF_RESPONDER_FLAGS_SPECIFIC_CALIB_MODE = BIT(8),
	IWL_TOF_RESPONDER_FLAGS_FAST_ALGO_SUPPORT = BIT(9),
	IWL_TOF_RESPONDER_FLAGS_RETRY_ON_ALGO_FAIL = BIT(10),
	IWL_TOF_RESPONDER_FLAGS_FTM_TX_ANT = RATE_MCS_ANT_ABC_MSK,
	IWL_TOF_RESPONDER_FLAGS_NDP_SUPPORT = BIT(24),
	IWL_TOF_RESPONDER_FLAGS_LMR_FEEDBACK = BIT(25),
	IWL_TOF_RESPONDER_FLAGS_SESSION_ID = BIT(27),
};
	IWL_LOCATION_BW_20MHZ,
	IWL_LOCATION_BW_40MHZ,
	IWL_LOCATION_BW_80MHZ,
};

#define TK_11AZ_LEN	32

	u8 reserved[3];
	u8 rx_pn[IEEE80211_CCMP_PN_LEN];
	u8 tx_pn[IEEE80211_CCMP_PN_LEN];
} __packed; /* LOCATION_RANGE_RSP_AP_ETRY_NTFY_API_S_VER_6 */

/**
 * enum iwl_tof_response_status - tof response status
 *
	u8 last_report;
	u8 reserved;
	struct iwl_tof_range_rsp_ap_entry_ntfy_v6 ap[IWL_MVM_TOF_MAX_APS];
} __packed; /* LOCATION_RANGE_RSP_NTFY_API_S_VER_8 */

#define IWL_MVM_TOF_MCSI_BUF_SIZE  (245)
/**
 * struct iwl_tof_mcsi_notif - used for debug