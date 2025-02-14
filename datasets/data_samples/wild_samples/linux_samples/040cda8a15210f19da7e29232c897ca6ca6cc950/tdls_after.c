		switch (*pos) {
		case WLAN_EID_SUPP_RATES:
			if (pos[1] > 32)
				return;
			sta_ptr->tdls_cap.rates_len = pos[1];
			for (i = 0; i < pos[1]; i++)
				sta_ptr->tdls_cap.rates[i] = pos[i + 2];
			break;

		case WLAN_EID_EXT_SUPP_RATES:
			if (pos[1] > 32)
				return;
			basic = sta_ptr->tdls_cap.rates_len;
			if (pos[1] > 32 - basic)
				return;
			for (i = 0; i < pos[1]; i++)
				sta_ptr->tdls_cap.rates[basic + i] = pos[i + 2];
			sta_ptr->tdls_cap.rates_len += pos[1];
			break;
		case WLAN_EID_HT_CAPABILITY:
			if (pos > end - sizeof(struct ieee80211_ht_cap) - 2)
				return;
			if (pos[1] != sizeof(struct ieee80211_ht_cap))
				return;
			/* copy the ie's value into ht_capb*/
			memcpy((u8 *)&sta_ptr->tdls_cap.ht_capb, pos + 2,
			       sizeof(struct ieee80211_ht_cap));
			sta_ptr->is_11n_enabled = 1;
			break;
		case WLAN_EID_HT_OPERATION:
			if (pos > end -
			    sizeof(struct ieee80211_ht_operation) - 2)
				return;
			if (pos[1] != sizeof(struct ieee80211_ht_operation))
				return;
			/* copy the ie's value into ht_oper*/
			memcpy(&sta_ptr->tdls_cap.ht_oper, pos + 2,
			       sizeof(struct ieee80211_ht_operation));
			break;
		case WLAN_EID_BSS_COEX_2040:
			if (pos > end - 3)
				return;
			if (pos[1] != 1)
				return;
			sta_ptr->tdls_cap.coex_2040 = pos[2];
			break;
		case WLAN_EID_EXT_CAPABILITY:
			if (pos > end - sizeof(struct ieee_types_header))
				return;
			if (pos[1] < sizeof(struct ieee_types_header))
				return;
			if (pos[1] > 8)
				return;
			memcpy((u8 *)&sta_ptr->tdls_cap.extcap, pos,
			       sizeof(struct ieee_types_header) +
			       min_t(u8, pos[1], 8));
			break;
		case WLAN_EID_RSN:
			if (pos > end - sizeof(struct ieee_types_header))
				return;
			if (pos[1] < sizeof(struct ieee_types_header))
				return;
			if (pos[1] > IEEE_MAX_IE_SIZE -
			    sizeof(struct ieee_types_header))
				return;
			memcpy((u8 *)&sta_ptr->tdls_cap.rsn_ie, pos,
			       sizeof(struct ieee_types_header) +
			       min_t(u8, pos[1], IEEE_MAX_IE_SIZE -
				     sizeof(struct ieee_types_header)));
			break;
		case WLAN_EID_QOS_CAPA:
			if (pos > end - 3)
				return;
			if (pos[1] != 1)
				return;
			sta_ptr->tdls_cap.qos_info = pos[2];
			break;
		case WLAN_EID_VHT_OPERATION:
			if (priv->adapter->is_hw_11ac_capable) {
				if (pos > end -
				    sizeof(struct ieee80211_vht_operation) - 2)
					return;
				if (pos[1] !=
				    sizeof(struct ieee80211_vht_operation))
					return;
				/* copy the ie's value into vhtoper*/
				memcpy(&sta_ptr->tdls_cap.vhtoper, pos + 2,
				       sizeof(struct ieee80211_vht_operation));
			}
			break;
		case WLAN_EID_VHT_CAPABILITY:
			if (priv->adapter->is_hw_11ac_capable) {
				if (pos > end -
				    sizeof(struct ieee80211_vht_cap) - 2)
					return;
				if (pos[1] != sizeof(struct ieee80211_vht_cap))
					return;
				/* copy the ie's value into vhtcap*/
				memcpy((u8 *)&sta_ptr->tdls_cap.vhtcap, pos + 2,
				       sizeof(struct ieee80211_vht_cap));
				sta_ptr->is_11ac_enabled = 1;
			}
			break;
		case WLAN_EID_AID:
			if (priv->adapter->is_hw_11ac_capable) {
				if (pos > end - 4)
					return;
				if (pos[1] != 2)
					return;
				sta_ptr->tdls_cap.aid =
					get_unaligned_le16((pos + 2));
			}
			break;
		default:
			break;
		}