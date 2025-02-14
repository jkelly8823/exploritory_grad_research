	int hw, ap, ap_max = ie[1];
	u8 hw_rate;

	/* Advance past IE header */
	ie += 2;

	lbs_deb_hex(LBS_DEB_ASSOC, "AP IE Rates", (u8 *) ie, ap_max);
	struct cmd_ds_802_11_ad_hoc_join cmd;
	u8 preamble = RADIO_PREAMBLE_SHORT;
	int ret = 0;

	/* TODO: set preamble based on scan result */
	ret = lbs_set_radio(priv, preamble, 1);
	if (ret)
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