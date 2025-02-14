 * Copyright(c) 2005 - 2014, 2018 - 2021 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
static inline u8 rs_extract_rate(u32 rate_n_flags)
{
	/* also works for HT because bits 7:6 are zero there */
	return (u8)(rate_n_flags & RATE_LEGACY_RATE_MSK_V1);
}

static int iwl_hwrate_to_plcp_idx(u32 rate_n_flags)
{
	int idx = 0;

	if (rate_n_flags & RATE_MCS_HT_MSK_V1) {
		idx = rate_n_flags & RATE_HT_MCS_RATE_CODE_MSK_V1;
		idx += IWL_RATE_MCS_0_INDEX;

		/* skip 9M not supported in HT*/
		if (idx >= IWL_RATE_9M_INDEX)
			idx += 1;
		if ((idx >= IWL_FIRST_HT_RATE) && (idx <= IWL_LAST_HT_RATE))
			return idx;
	} else if (rate_n_flags & RATE_MCS_VHT_MSK_V1 ||
		   rate_n_flags & RATE_MCS_HE_MSK_V1) {
		idx = rate_n_flags & RATE_VHT_MCS_RATE_CODE_MSK;
		idx += IWL_RATE_MCS_0_INDEX;

		/* skip 9M not supported in VHT*/
			idx++;
		if ((idx >= IWL_FIRST_VHT_RATE) && (idx <= IWL_LAST_VHT_RATE))
			return idx;
		if ((rate_n_flags & RATE_MCS_HE_MSK_V1) &&
		    idx <= IWL_LAST_HE_RATE)
			return idx;
	} else {
		/* legacy rate format, search for match in table */

	{0, 0, 0, 0, 971, 0, 1925, 2861, 3779, 5574, 7304, 8147, 8976, 10592, 11640},
};

#define MCS_INDEX_PER_STREAM	(8)

static const char *rs_pretty_lq_type(enum iwl_table_type type)
{
	static const char * const lq_types[] = {
		[LQ_NONE] = "NONE",
		rate_str = "BAD_RATE";

	sprintf(buf, "(%s|%s|%s)", rs_pretty_lq_type(rate->type),
		iwl_rs_pretty_ant(rate->ant), rate_str);
	return buf;
}

static inline void rs_dump_rate(struct iwl_mvm *mvm, const struct rs_rate *rate,
static inline int get_num_of_ant_from_rate(u32 rate_n_flags)
{
	return !!(rate_n_flags & RATE_MCS_ANT_A_MSK) +
	       !!(rate_n_flags & RATE_MCS_ANT_B_MSK);
}

/*
 * Static function to get the expected throughput from an iwl_scale_tbl_info
	int index = rate->index;

	ucode_rate |= ((rate->ant << RATE_MCS_ANT_POS) &
			 RATE_MCS_ANT_AB_MSK);

	if (is_legacy(rate)) {
		ucode_rate |= iwl_rates[index].plcp;
		if (index >= IWL_FIRST_CCK_RATE && index <= IWL_LAST_CCK_RATE)
			ucode_rate |= RATE_MCS_CCK_MSK_V1;
		return ucode_rate;
	}

	/* set RTS protection for all non legacy rates
			IWL_ERR(mvm, "Invalid HT rate index %d\n", index);
			index = IWL_LAST_HT_RATE;
		}
		ucode_rate |= RATE_MCS_HT_MSK_V1;

		if (is_ht_siso(rate))
			ucode_rate |= iwl_rates[index].plcp_ht_siso;
		else if (is_ht_mimo2(rate))
			IWL_ERR(mvm, "Invalid VHT rate index %d\n", index);
			index = IWL_LAST_VHT_RATE;
		}
		ucode_rate |= RATE_MCS_VHT_MSK_V1;
		if (is_vht_siso(rate))
			ucode_rate |= iwl_rates[index].plcp_vht_siso;
		else if (is_vht_mimo2(rate))
			ucode_rate |= iwl_rates[index].plcp_vht_mimo2;

	ucode_rate |= rate->bw;
	if (rate->sgi)
		ucode_rate |= RATE_MCS_SGI_MSK_V1;
	if (rate->ldpc)
		ucode_rate |= RATE_MCS_LDPC_MSK_V1;

	return ucode_rate;
}

				   enum nl80211_band band,
				   struct rs_rate *rate)
{
	u32 ant_msk = ucode_rate & RATE_MCS_ANT_AB_MSK;
	u8 num_of_ant = get_num_of_ant_from_rate(ucode_rate);
	u8 nss;

	memset(rate, 0, sizeof(*rate));
	rate->ant = (ant_msk >> RATE_MCS_ANT_POS);

	/* Legacy */
	if (!(ucode_rate & RATE_MCS_HT_MSK_V1) &&
	    !(ucode_rate & RATE_MCS_VHT_MSK_V1) &&
	    !(ucode_rate & RATE_MCS_HE_MSK_V1)) {
		if (num_of_ant == 1) {
			if (band == NL80211_BAND_5GHZ)
				rate->type = LQ_LEGACY_A;
			else
	}

	/* HT, VHT or HE */
	if (ucode_rate & RATE_MCS_SGI_MSK_V1)
		rate->sgi = true;
	if (ucode_rate & RATE_MCS_LDPC_MSK_V1)
		rate->ldpc = true;
	if (ucode_rate & RATE_MCS_STBC_MSK)
		rate->stbc = true;
	if (ucode_rate & RATE_MCS_BF_MSK)
		rate->bfer = true;

	rate->bw = ucode_rate & RATE_MCS_CHAN_WIDTH_MSK_V1;

	if (ucode_rate & RATE_MCS_HT_MSK_V1) {
		nss = ((ucode_rate & RATE_HT_MCS_NSS_MSK_V1) >>
		       RATE_HT_MCS_NSS_POS_V1) + 1;

		if (nss == 1) {
			rate->type = LQ_HT_SISO;
			WARN_ONCE(!rate->stbc && !rate->bfer && num_of_ant != 1,
		} else {
			WARN_ON_ONCE(1);
		}
	} else if (ucode_rate & RATE_MCS_VHT_MSK_V1) {
		nss = ((ucode_rate & RATE_VHT_MCS_NSS_MSK) >>
		       RATE_VHT_MCS_NSS_POS) + 1;

		if (nss == 1) {
		} else {
			WARN_ON_ONCE(1);
		}
	} else if (ucode_rate & RATE_MCS_HE_MSK_V1) {
		nss = ((ucode_rate & RATE_VHT_MCS_NSS_MSK) >>
		      RATE_VHT_MCS_NSS_POS) + 1;

		if (nss == 1) {
{
	u8 new_ant_type;

	if (!rs_is_valid_ant(valid_ant, rate->ant))
		return 0;

	new_ant_type = ant_toggle_lookup[rate->ant];
	}

	IWL_DEBUG_RATE(mvm, "Best ANT: %s Best RSSI: %d\n",
		       iwl_rs_pretty_ant(best_ant), best_rssi);

	if (best_ant != ANT_A && best_ant != ANT_B)
		rate->ant = first_antenna(valid_tx_ant);
	else
	lq_sta->pers.chains = rx_status->chains;
	lq_sta->pers.chain_signal[0] = rx_status->chain_signal[0];
	lq_sta->pers.chain_signal[1] = rx_status->chain_signal[1];
	lq_sta->pers.last_rssi = S8_MIN;

	for (i = 0; i < ARRAY_SIZE(lq_sta->pers.chain_signal); i++) {
		if (!(lq_sta->pers.chains & BIT(i)))
		return;

	lq_sta = mvm_sta;
	iwl_mvm_hwrate_to_tx_rate_v1(lq_sta->last_rate_n_flags,
				     info->band, &info->control.rates[0]);
	info->control.rates[0].count = 1;

	/* Report the optimal rate based on rssi and STA caps if we haven't
	 * converged yet (too little traffic) or exploring other modulations
		optimal_rate = rs_get_optimal_rate(mvm, lq_sta);
		last_ucode_rate = ucode_rate_from_rs_rate(mvm,
							  optimal_rate);
		iwl_mvm_hwrate_to_tx_rate_v1(last_ucode_rate, info->band,
					     &txrc->reported_rate);
	}
}

static void *rs_drv_alloc_sta(void *mvm_rate, struct ieee80211_sta *sta,

	mvm->drv_rx_stats.success_frames++;

	switch (rate & RATE_MCS_CHAN_WIDTH_MSK_V1) {
	case RATE_MCS_CHAN_WIDTH_20:
		mvm->drv_rx_stats.bw_20_frames++;
		break;
	case RATE_MCS_CHAN_WIDTH_40:
		WARN_ONCE(1, "bad BW. rate 0x%x", rate);
	}

	if (rate & RATE_MCS_HT_MSK_V1) {
		mvm->drv_rx_stats.ht_frames++;
		nss = ((rate & RATE_HT_MCS_NSS_MSK_V1) >> RATE_HT_MCS_NSS_POS_V1) + 1;
	} else if (rate & RATE_MCS_VHT_MSK_V1) {
		mvm->drv_rx_stats.vht_frames++;
		nss = ((rate & RATE_VHT_MCS_NSS_MSK) >>
		       RATE_VHT_MCS_NSS_POS) + 1;
	} else {
	else if (nss == 2)
		mvm->drv_rx_stats.mimo2_frames++;

	if (rate & RATE_MCS_SGI_MSK_V1)
		mvm->drv_rx_stats.sgi_frames++;
	else
		mvm->drv_rx_stats.ngi_frames++;

	int i;
	int num_rates = ARRAY_SIZE(lq_cmd->rs_table);
	__le32 ucode_rate_le32 = cpu_to_le32(ucode_rate);
	u8 ant = (ucode_rate & RATE_MCS_ANT_AB_MSK) >> RATE_MCS_ANT_POS;

	for (i = 0; i < num_rates; i++)
		lq_cmd->rs_table[i] = ucode_rate_le32;

	IWL_DEBUG_RATE(mvm, "leave\n");
}

int rs_pretty_print_rate_v1(char *buf, int bufsz, const u32 rate)
{

	char *type;
	u8 mcs = 0, nss = 0;
	u8 ant = (rate & RATE_MCS_ANT_AB_MSK) >> RATE_MCS_ANT_POS;
	u32 bw = (rate & RATE_MCS_CHAN_WIDTH_MSK_V1) >>
		RATE_MCS_CHAN_WIDTH_POS;

	if (!(rate & RATE_MCS_HT_MSK_V1) &&
	    !(rate & RATE_MCS_VHT_MSK_V1) &&
	    !(rate & RATE_MCS_HE_MSK_V1)) {
		int index = iwl_hwrate_to_plcp_idx(rate);

		return scnprintf(buf, bufsz, "Legacy | ANT: %s Rate: %s Mbps",
				 iwl_rs_pretty_ant(ant),
				 index == IWL_RATE_INVALID ? "BAD" :
				 iwl_rate_mcs(index)->mbps);
	}

	if (rate & RATE_MCS_VHT_MSK_V1) {
		type = "VHT";
		mcs = rate & RATE_VHT_MCS_RATE_CODE_MSK;
		nss = ((rate & RATE_VHT_MCS_NSS_MSK)
		       >> RATE_VHT_MCS_NSS_POS) + 1;
	} else if (rate & RATE_MCS_HT_MSK_V1) {
		type = "HT";
		mcs = rate & RATE_HT_MCS_INDEX_MSK_V1;
		nss = ((rate & RATE_HT_MCS_NSS_MSK_V1)
		       >> RATE_HT_MCS_NSS_POS_V1) + 1;
	} else if (rate & RATE_MCS_HE_MSK_V1) {
		type = "HE";
		mcs = rate & RATE_VHT_MCS_RATE_CODE_MSK;
		nss = ((rate & RATE_VHT_MCS_NSS_MSK)
		       >> RATE_VHT_MCS_NSS_POS) + 1;
		type = "Unknown"; /* shouldn't happen */
	}

	return scnprintf(buf, bufsz,
			 "0x%x: %s | ANT: %s BW: %s MCS: %d NSS: %d %s%s%s%s%s",
			 rate, type, iwl_rs_pretty_ant(ant), iwl_rs_pretty_bw(bw), mcs, nss,
			 (rate & RATE_MCS_SGI_MSK_V1) ? "SGI " : "NGI ",
			 (rate & RATE_MCS_STBC_MSK) ? "STBC " : "",
			 (rate & RATE_MCS_LDPC_MSK_V1) ? "LDPC " : "",
			 (rate & RATE_HE_DUAL_CARRIER_MODE_MSK) ? "DCM " : "",
			 (rate & RATE_MCS_BF_MSK) ? "BF " : "");
}

			  lq_sta->active_legacy_rate);
	desc += scnprintf(buff + desc, bufsz - desc, "fixed rate 0x%X\n",
			  lq_sta->pers.dbg_fixed_rate);
	desc += scnprintf(buff + desc, bufsz - desc, "valid_tx_ant %s%s\n",
	    (iwl_mvm_get_valid_tx_ant(mvm) & ANT_A) ? "ANT_A," : "",
	    (iwl_mvm_get_valid_tx_ant(mvm) & ANT_B) ? "ANT_B," : "");
	desc += scnprintf(buff + desc, bufsz - desc, "lq type %s\n",
			  (is_legacy(rate)) ? "legacy" :
			  is_vht(rate) ? "VHT" : "HT");
	if (!is_legacy(rate)) {

		desc += scnprintf(buff + desc, bufsz - desc,
				  " rate[%d] 0x%X ", i, r);
		desc += rs_pretty_print_rate_v1(buff + desc, bufsz - desc, r);
		if (desc < bufsz - 1)
			buff[desc++] = '\n';
	}
