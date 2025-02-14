	return pn;
}

static bool ath10k_htt_rx_h_frag_multicast_check(struct ath10k *ar,
						 struct sk_buff *skb,
						 u16 offset)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + offset);
	return !is_multicast_ether_addr(hdr->addr1);
}

static bool ath10k_htt_rx_h_frag_pn_check(struct ath10k *ar,
					  struct sk_buff *skb,
					  u16 peer_id,
					  u16 offset,
	bool is_decrypted;
	bool is_mgmt;
	u32 attention;
	bool frag_pn_check = true, multicast_check = true;

	if (skb_queue_empty(amsdu))
		return;

								      0,
								      enctype);

		if (frag)
			multicast_check = ath10k_htt_rx_h_frag_multicast_check(ar,
									       msdu,
									       0);

		if (!frag_pn_check || !multicast_check) {
			/* Discard the fragment with invalid PN or multicast DA
			 */
			temp = msdu->prev;
			__skb_unlink(msdu, amsdu);
			dev_kfree_skb_any(msdu);
			msdu = temp;
			frag_pn_check = true;
			multicast_check = true;
			continue;
		}

		ath10k_htt_rx_h_csum_offload(msdu);