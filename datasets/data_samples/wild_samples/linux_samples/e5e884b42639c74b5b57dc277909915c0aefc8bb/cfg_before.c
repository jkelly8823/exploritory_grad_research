{
	int hw, ap, ap_max = ie[1];
	u8 hw_rate;

	/* Advance past IE header */
	ie += 2;

	lbs_deb_hex(LBS_DEB_ASSOC, "AP IE Rates", (u8 *) ie, ap_max);

	for (hw = 0; hw < ARRAY_SIZE(lbs_rates); hw++) {
		hw_rate = lbs_rates[hw].bitrate / 5;
		for (ap = 0; ap < ap_max; ap++) {
			if (hw_rate == (ie[ap] & 0x7f)) {
				*tlv++ = ie[ap];
				*nrates = *nrates + 1;
			}
		}
	}
{
	const u8 *rates_eid;
	struct cmd_ds_802_11_ad_hoc_join cmd;
	u8 preamble = RADIO_PREAMBLE_SHORT;
	int ret = 0;

	/* TODO: set preamble based on scan result */
	ret = lbs_set_radio(priv, preamble, 1);
	if (ret)
		goto out;

	/*
	 * Example CMD_802_11_AD_HOC_JOIN command:
	 *
	 * command         2c 00         CMD_802_11_AD_HOC_JOIN
	 * size            65 00
	 * sequence        xx xx
	 * result          00 00
	 * bssid           02 27 27 97 2f 96
	 * ssid            49 42 53 53 00 00 00 00
	 *                 00 00 00 00 00 00 00 00
	 *                 00 00 00 00 00 00 00 00
	 *                 00 00 00 00 00 00 00 00
	 * type            02            CMD_BSS_TYPE_IBSS
	 * beacon period   64 00
	 * dtim period     00
	 * timestamp       00 00 00 00 00 00 00 00
	 * localtime       00 00 00 00 00 00 00 00
	 * IE DS           03
	 * IE DS len       01
	 * IE DS channel   01
	 * reserveed       00 00 00 00
	 * IE IBSS         06
	 * IE IBSS len     02
	 * IE IBSS atim    00 00
	 * reserved        00 00 00 00
	 * capability      02 00
	 * rates           82 84 8b 96 0c 12 18 24 30 48 60 6c 00
	 * fail timeout    ff 00
	 * probe delay     00 00
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.hdr.size = cpu_to_le16(sizeof(cmd));

	memcpy(cmd.bss.bssid, bss->bssid, ETH_ALEN);
	memcpy(cmd.bss.ssid, params->ssid, params->ssid_len);
	cmd.bss.type = CMD_BSS_TYPE_IBSS;
	cmd.bss.beaconperiod = cpu_to_le16(params->beacon_interval);
	cmd.bss.ds.header.id = WLAN_EID_DS_PARAMS;
	cmd.bss.ds.header.len = 1;
	cmd.bss.ds.channel = params->chandef.chan->hw_value;
	cmd.bss.ibss.header.id = WLAN_EID_IBSS_PARAMS;
	cmd.bss.ibss.header.len = 2;
	cmd.bss.ibss.atimwindow = 0;
	cmd.bss.capability = cpu_to_le16(bss->capability & CAPINFO_MASK);

	/* set rates to the intersection of our rates and the rates in the
	   bss */
	rcu_read_lock();
	rates_eid = ieee80211_bss_get_ie(bss, WLAN_EID_SUPP_RATES);
	if (!rates_eid) {
		lbs_add_rates(cmd.bss.rates);
	} else {
		int hw, i;
		u8 rates_max = rates_eid[1];
		u8 *rates = cmd.bss.rates;
		for (hw = 0; hw < ARRAY_SIZE(lbs_rates); hw++) {
			u8 hw_rate = lbs_rates[hw].bitrate / 5;
			for (i = 0; i < rates_max; i++) {
				if (hw_rate == (rates_eid[i+2] & 0x7f)) {
					u8 rate = rates_eid[i+2];
					if (rate == 0x02 || rate == 0x04 ||
					    rate == 0x0b || rate == 0x16)
						rate |= 0x80;
					*rates++ = rate;
				}
			}
		}
	}
	rcu_read_unlock();

	/* Only v8 and below support setting this */
	if (MRVL_FW_MAJOR_REV(priv->fwrelease) <= 8) {
		cmd.failtimeout = cpu_to_le16(MRVDRV_ASSOCIATION_TIME_OUT);
		cmd.probedelay = cpu_to_le16(CMD_SCAN_PROBE_DELAY_TIME);
	}
	ret = lbs_cmd_with_response(priv, CMD_802_11_AD_HOC_JOIN, &cmd);
	if (ret)
		goto out;

	/*
	 * This is a sample response to CMD_802_11_AD_HOC_JOIN:
	 *
	 * response        2c 80
	 * size            09 00
	 * sequence        xx xx
	 * result          00 00
	 * reserved        00
	 */
	lbs_join_post(priv, params, bss->bssid, bss->capability);

 out:
	return ret;
}
	if (!rates_eid) {
		lbs_add_rates(cmd.bss.rates);
	} else {
		int hw, i;
		u8 rates_max = rates_eid[1];
		u8 *rates = cmd.bss.rates;
		for (hw = 0; hw < ARRAY_SIZE(lbs_rates); hw++) {
			u8 hw_rate = lbs_rates[hw].bitrate / 5;
			for (i = 0; i < rates_max; i++) {
				if (hw_rate == (rates_eid[i+2] & 0x7f)) {
					u8 rate = rates_eid[i+2];
					if (rate == 0x02 || rate == 0x04 ||
					    rate == 0x0b || rate == 0x16)
						rate |= 0x80;
					*rates++ = rate;
				}
			}
		}
	}