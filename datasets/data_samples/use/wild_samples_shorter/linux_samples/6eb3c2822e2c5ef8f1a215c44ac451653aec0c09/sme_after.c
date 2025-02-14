	}
#endif

	if (!bss && (status == WLAN_STATUS_SUCCESS)) {
		WARN_ON_ONCE(!wiphy_to_dev(wdev->wiphy)->ops->connect);
		bss = cfg80211_get_bss(wdev->wiphy, NULL, bssid,
				       wdev->ssid, wdev->ssid_len,
				       WLAN_CAPABILITY_ESS,
				       WLAN_CAPABILITY_ESS);
		if (bss)
			cfg80211_hold_bss(bss_from_pub(bss));
	}

	if (wdev->current_bss) {
		cfg80211_unhold_bss(wdev->current_bss);
		cfg80211_put_bss(wdev->wiphy, &wdev->current_bss->pub);
		wdev->current_bss = NULL;
		return;
	}

	if (WARN_ON(!bss))
		return;

	wdev->current_bss = bss_from_pub(bss);

	cfg80211_upload_connect_keys(wdev);