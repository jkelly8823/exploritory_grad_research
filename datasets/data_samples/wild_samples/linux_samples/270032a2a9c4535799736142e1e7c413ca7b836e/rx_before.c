// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007-2010	Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2013-2014  Intel Mobile Communications GmbH
 * Copyright(c) 2015 - 2017 Intel Deutschland GmbH
 * Copyright (C) 2018-2020 Intel Corporation
 */

#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rcupdate.h>
#include <linux/export.h>
#include <linux/kcov.h>
#include <linux/bitops.h>
#include <net/mac80211.h>
#include <net/ieee80211_radiotap.h>
#include <asm/unaligned.h>

#include "ieee80211_i.h"
#include "driver-ops.h"
#include "led.h"
#include "mesh.h"
#include "wep.h"
#include "wpa.h"
#include "tkip.h"
#include "wme.h"
#include "rate.h"

/*
 * monitor mode reception
 *
 * This function cleans up the SKB, i.e. it removes all the stuff
 * only useful for monitoring.
 */
static struct sk_buff *ieee80211_clean_skb(struct sk_buff *skb,
					   unsigned int present_fcs_len,
					   unsigned int rtap_space)
{
	struct ieee80211_hdr *hdr;
	unsigned int hdrlen;
	__le16 fc;

	if (present_fcs_len)
		__pskb_trim(skb, skb->len - present_fcs_len);
	__pskb_pull(skb, rtap_space);

	hdr = (void *)skb->data;
	fc = hdr->frame_control;

	/*
	 * Remove the HT-Control field (if present) on management
	 * frames after we've sent the frame to monitoring. We
	 * (currently) don't need it, and don't properly parse
	 * frames with it present, due to the assumption of a
	 * fixed management header length.
	 */
	if (likely(!ieee80211_is_mgmt(fc) || !ieee80211_has_order(fc)))
		return skb;

	hdrlen = ieee80211_hdrlen(fc);
	hdr->frame_control &= ~cpu_to_le16(IEEE80211_FCTL_ORDER);

	if (!pskb_may_pull(skb, hdrlen)) {
		dev_kfree_skb(skb);
		return NULL;
	}

	memmove(skb->data + IEEE80211_HT_CTL_LEN, skb->data,
		hdrlen - IEEE80211_HT_CTL_LEN);
	__pskb_pull(skb, IEEE80211_HT_CTL_LEN);

	return skb;
}
		switch (rx->sdata->vif.type) {
		case NL80211_IFTYPE_AP_VLAN:
			if (!rx->sdata->u.vlan.sta)
				return RX_DROP_UNUSABLE;
			break;
		case NL80211_IFTYPE_STATION:
			if (!rx->sdata->u.mgd.use_4addr)
				return RX_DROP_UNUSABLE;
			break;
		default:
			return RX_DROP_UNUSABLE;
		}
	}

	if (is_multicast_ether_addr(hdr->addr1))
		return RX_DROP_UNUSABLE;

	return __ieee80211_rx_h_amsdu(rx, 0);
}

#ifdef CONFIG_MAC80211_MESH
static ieee80211_rx_result
ieee80211_rx_h_mesh_fwding(struct ieee80211_rx_data *rx)
{
	struct ieee80211_hdr *fwd_hdr, *hdr;
	struct ieee80211_tx_info *info;
	struct ieee80211s_hdr *mesh_hdr;
	struct sk_buff *skb = rx->skb, *fwd_skb;
	struct ieee80211_local *local = rx->local;
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_if_mesh *ifmsh = &sdata->u.mesh;
	u16 ac, q, hdrlen;
	int tailroom = 0;

	hdr = (struct ieee80211_hdr *) skb->data;
	hdrlen = ieee80211_hdrlen(hdr->frame_control);

	/* make sure fixed part of mesh header is there, also checks skb len */
	if (!pskb_may_pull(rx->skb, hdrlen + 6))
		return RX_DROP_MONITOR;

	mesh_hdr = (struct ieee80211s_hdr *) (skb->data + hdrlen);

	/* make sure full mesh header is there, also checks skb len */
	if (!pskb_may_pull(rx->skb,
			   hdrlen + ieee80211_get_mesh_hdrlen(mesh_hdr)))
		return RX_DROP_MONITOR;

	/* reload pointers */
	hdr = (struct ieee80211_hdr *) skb->data;
	mesh_hdr = (struct ieee80211s_hdr *) (skb->data + hdrlen);

	if (ieee80211_drop_unencrypted(rx, hdr->frame_control))
		return RX_DROP_MONITOR;

	/* frame is in RMC, don't forward */
	if (ieee80211_is_data(hdr->frame_control) &&
	    is_multicast_ether_addr(hdr->addr1) &&
	    mesh_rmc_check(rx->sdata, hdr->addr3, mesh_hdr))
		return RX_DROP_MONITOR;

	if (!ieee80211_is_data(hdr->frame_control))
		return RX_CONTINUE;

	if (!mesh_hdr->ttl)
		return RX_DROP_MONITOR;

	if (mesh_hdr->flags & MESH_FLAGS_AE) {
		struct mesh_path *mppath;
		char *proxied_addr;
		char *mpp_addr;

		if (is_multicast_ether_addr(hdr->addr1)) {
			mpp_addr = hdr->addr3;
			proxied_addr = mesh_hdr->eaddr1;
		} else if ((mesh_hdr->flags & MESH_FLAGS_AE) ==
			    MESH_FLAGS_AE_A5_A6) {
			/* has_a4 already checked in ieee80211_rx_mesh_check */
			mpp_addr = hdr->addr4;
			proxied_addr = mesh_hdr->eaddr2;
		} else {
			return RX_DROP_MONITOR;
		}

		rcu_read_lock();
		mppath = mpp_path_lookup(sdata, proxied_addr);
		if (!mppath) {
			mpp_path_add(sdata, proxied_addr, mpp_addr);
		} else {
			spin_lock_bh(&mppath->state_lock);
			if (!ether_addr_equal(mppath->mpp, mpp_addr))
				memcpy(mppath->mpp, mpp_addr, ETH_ALEN);
			mppath->exp_time = jiffies;
			spin_unlock_bh(&mppath->state_lock);
		}
		rcu_read_unlock();
	}

	/* Frame has reached destination.  Don't forward */
	if (!is_multicast_ether_addr(hdr->addr1) &&
	    ether_addr_equal(sdata->vif.addr, hdr->addr3))
		return RX_CONTINUE;

	ac = ieee80211_select_queue_80211(sdata, skb, hdr);
	q = sdata->vif.hw_queue[ac];
	if (ieee80211_queue_stopped(&local->hw, q)) {
		IEEE80211_IFSTA_MESH_CTR_INC(ifmsh, dropped_frames_congestion);
		return RX_DROP_MONITOR;
	}
	skb_set_queue_mapping(skb, q);

	if (!--mesh_hdr->ttl) {
		if (!is_multicast_ether_addr(hdr->addr1))
			IEEE80211_IFSTA_MESH_CTR_INC(ifmsh,
						     dropped_frames_ttl);
		goto out;
	}

	if (!ifmsh->mshcfg.dot11MeshForwarding)
		goto out;

	if (sdata->crypto_tx_tailroom_needed_cnt)
		tailroom = IEEE80211_ENCRYPT_TAILROOM;

	fwd_skb = skb_copy_expand(skb, local->tx_headroom +
				       sdata->encrypt_headroom,
				  tailroom, GFP_ATOMIC);
	if (!fwd_skb)
		goto out;

	fwd_hdr =  (struct ieee80211_hdr *) fwd_skb->data;
	fwd_hdr->frame_control &= ~cpu_to_le16(IEEE80211_FCTL_RETRY);
	info = IEEE80211_SKB_CB(fwd_skb);
	memset(info, 0, sizeof(*info));
	info->control.flags |= IEEE80211_TX_INTCFL_NEED_TXPROCESSING;
	info->control.vif = &rx->sdata->vif;
	info->control.jiffies = jiffies;
	if (is_multicast_ether_addr(fwd_hdr->addr1)) {
		IEEE80211_IFSTA_MESH_CTR_INC(ifmsh, fwded_mcast);
		memcpy(fwd_hdr->addr2, sdata->vif.addr, ETH_ALEN);
		/* update power mode indication when forwarding */
		ieee80211_mps_set_frame_flags(sdata, NULL, fwd_hdr);
	} else if (!mesh_nexthop_lookup(sdata, fwd_skb)) {
		/* mesh power mode flags updated in mesh_nexthop_lookup */
		IEEE80211_IFSTA_MESH_CTR_INC(ifmsh, fwded_unicast);
	} else {
		/* unable to resolve next hop */
		mesh_path_error_tx(sdata, ifmsh->mshcfg.element_ttl,
				   fwd_hdr->addr3, 0,
				   WLAN_REASON_MESH_PATH_NOFORWARD,
				   fwd_hdr->addr2);
		IEEE80211_IFSTA_MESH_CTR_INC(ifmsh, dropped_frames_no_route);
		kfree_skb(fwd_skb);
		return RX_DROP_MONITOR;
	}

	IEEE80211_IFSTA_MESH_CTR_INC(ifmsh, fwded_frames);
	ieee80211_add_pending_skb(local, fwd_skb);
 out:
	if (is_multicast_ether_addr(hdr->addr1))
		return RX_CONTINUE;
	return RX_DROP_MONITOR;
}
#endif

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_data(struct ieee80211_rx_data *rx)
{
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_local *local = rx->local;
	struct net_device *dev = sdata->dev;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)rx->skb->data;
	__le16 fc = hdr->frame_control;
	bool port_control;
	int err;

	if (unlikely(!ieee80211_is_data(hdr->frame_control)))
		return RX_CONTINUE;

	if (unlikely(!ieee80211_is_data_present(hdr->frame_control)))
		return RX_DROP_MONITOR;

	/*
	 * Send unexpected-4addr-frame event to hostapd. For older versions,
	 * also drop the frame to cooked monitor interfaces.
	 */
	if (ieee80211_has_a4(hdr->frame_control) &&
	    sdata->vif.type == NL80211_IFTYPE_AP) {
		if (rx->sta &&
		    !test_and_set_sta_flag(rx->sta, WLAN_STA_4ADDR_EVENT))
			cfg80211_rx_unexpected_4addr_frame(
				rx->sdata->dev, rx->sta->sta.addr, GFP_ATOMIC);
		return RX_DROP_MONITOR;
	}

	err = __ieee80211_data_to_8023(rx, &port_control);
	if (unlikely(err))
		return RX_DROP_UNUSABLE;

	if (!ieee80211_frame_allowed(rx, fc))
		return RX_DROP_MONITOR;

	/* directly handle TDLS channel switch requests/responses */
	if (unlikely(((struct ethhdr *)rx->skb->data)->h_proto ==
						cpu_to_be16(ETH_P_TDLS))) {
		struct ieee80211_tdls_data *tf = (void *)rx->skb->data;

		if (pskb_may_pull(rx->skb,
				  offsetof(struct ieee80211_tdls_data, u)) &&
		    tf->payload_type == WLAN_TDLS_SNAP_RFTYPE &&
		    tf->category == WLAN_CATEGORY_TDLS &&
		    (tf->action_code == WLAN_TDLS_CHANNEL_SWITCH_REQUEST ||
		     tf->action_code == WLAN_TDLS_CHANNEL_SWITCH_RESPONSE)) {
			skb_queue_tail(&local->skb_queue_tdls_chsw, rx->skb);
			schedule_work(&local->tdls_chsw_work);
			if (rx->sta)
				rx->sta->rx_stats.packets++;

			return RX_QUEUED;
		}
	}

	if (rx->sdata->vif.type == NL80211_IFTYPE_AP_VLAN &&
	    unlikely(port_control) && sdata->bss) {
		sdata = container_of(sdata->bss, struct ieee80211_sub_if_data,
				     u.ap);
		dev = sdata->dev;
		rx->sdata = sdata;
	}

	rx->skb->dev = dev;

	if (!ieee80211_hw_check(&local->hw, SUPPORTS_DYNAMIC_PS) &&
	    local->ps_sdata && local->hw.conf.dynamic_ps_timeout > 0 &&
	    !is_multicast_ether_addr(
		    ((struct ethhdr *)rx->skb->data)->h_dest) &&
	    (!local->scanning &&
	     !test_bit(SDATA_STATE_OFFCHANNEL, &sdata->state)))
		mod_timer(&local->dynamic_ps_timer, jiffies +
			  msecs_to_jiffies(local->hw.conf.dynamic_ps_timeout));

	ieee80211_deliver_skb(rx);

	return RX_QUEUED;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_ctrl(struct ieee80211_rx_data *rx, struct sk_buff_head *frames)
{
	struct sk_buff *skb = rx->skb;
	struct ieee80211_bar *bar = (struct ieee80211_bar *)skb->data;
	struct tid_ampdu_rx *tid_agg_rx;
	u16 start_seq_num;
	u16 tid;

	if (likely(!ieee80211_is_ctl(bar->frame_control)))
		return RX_CONTINUE;

	if (ieee80211_is_back_req(bar->frame_control)) {
		struct {
			__le16 control, start_seq_num;
		} __packed bar_data;
		struct ieee80211_event event = {
			.type = BAR_RX_EVENT,
		};

		if (!rx->sta)
			return RX_DROP_MONITOR;

		if (skb_copy_bits(skb, offsetof(struct ieee80211_bar, control),
				  &bar_data, sizeof(bar_data)))
			return RX_DROP_MONITOR;

		tid = le16_to_cpu(bar_data.control) >> 12;

		if (!test_bit(tid, rx->sta->ampdu_mlme.agg_session_valid) &&
		    !test_and_set_bit(tid, rx->sta->ampdu_mlme.unexpected_agg))
			ieee80211_send_delba(rx->sdata, rx->sta->sta.addr, tid,
					     WLAN_BACK_RECIPIENT,
					     WLAN_REASON_QSTA_REQUIRE_SETUP);

		tid_agg_rx = rcu_dereference(rx->sta->ampdu_mlme.tid_rx[tid]);
		if (!tid_agg_rx)
			return RX_DROP_MONITOR;

		start_seq_num = le16_to_cpu(bar_data.start_seq_num) >> 4;
		event.u.ba.tid = tid;
		event.u.ba.ssn = start_seq_num;
		event.u.ba.sta = &rx->sta->sta;

		/* reset session timer */
		if (tid_agg_rx->timeout)
			mod_timer(&tid_agg_rx->session_timer,
				  TU_TO_EXP_TIME(tid_agg_rx->timeout));

		spin_lock(&tid_agg_rx->reorder_lock);
		/* release stored frames up to start of BAR */
		ieee80211_release_reorder_frames(rx->sdata, tid_agg_rx,
						 start_seq_num, frames);
		spin_unlock(&tid_agg_rx->reorder_lock);

		drv_event_callback(rx->local, rx->sdata, &event);

		kfree_skb(skb);
		return RX_QUEUED;
	}

	/*
	 * After this point, we only want management frames,
	 * so we can drop all remaining control frames to
	 * cooked monitor interfaces.
	 */
	return RX_DROP_MONITOR;
}

static void ieee80211_process_sa_query_req(struct ieee80211_sub_if_data *sdata,
					   struct ieee80211_mgmt *mgmt,
					   size_t len)
{
	struct ieee80211_local *local = sdata->local;
	struct sk_buff *skb;
	struct ieee80211_mgmt *resp;

	if (!ether_addr_equal(mgmt->da, sdata->vif.addr)) {
		/* Not to own unicast address */
		return;
	}

	if (!ether_addr_equal(mgmt->sa, sdata->u.mgd.bssid) ||
	    !ether_addr_equal(mgmt->bssid, sdata->u.mgd.bssid)) {
		/* Not from the current AP or not associated yet. */
		return;
	}

	if (len < 24 + 1 + sizeof(resp->u.action.u.sa_query)) {
		/* Too short SA Query request frame */
		return;
	}

	skb = dev_alloc_skb(sizeof(*resp) + local->hw.extra_tx_headroom);
	if (skb == NULL)
		return;

	skb_reserve(skb, local->hw.extra_tx_headroom);
	resp = skb_put_zero(skb, 24);
	memcpy(resp->da, mgmt->sa, ETH_ALEN);
	memcpy(resp->sa, sdata->vif.addr, ETH_ALEN);
	memcpy(resp->bssid, sdata->u.mgd.bssid, ETH_ALEN);
	resp->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
					  IEEE80211_STYPE_ACTION);
	skb_put(skb, 1 + sizeof(resp->u.action.u.sa_query));
	resp->u.action.category = WLAN_CATEGORY_SA_QUERY;
	resp->u.action.u.sa_query.action = WLAN_ACTION_SA_QUERY_RESPONSE;
	memcpy(resp->u.action.u.sa_query.trans_id,
	       mgmt->u.action.u.sa_query.trans_id,
	       WLAN_SA_QUERY_TR_ID_LEN);

	ieee80211_tx_skb(sdata, skb);
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_mgmt_check(struct ieee80211_rx_data *rx)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) rx->skb->data;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(rx->skb);

	if (ieee80211_is_s1g_beacon(mgmt->frame_control))
		return RX_CONTINUE;

	/*
	 * From here on, look only at management frames.
	 * Data and control frames are already handled,
	 * and unknown (reserved) frames are useless.
	 */
	if (rx->skb->len < 24)
		return RX_DROP_MONITOR;

	if (!ieee80211_is_mgmt(mgmt->frame_control))
		return RX_DROP_MONITOR;

	if (rx->sdata->vif.type == NL80211_IFTYPE_AP &&
	    ieee80211_is_beacon(mgmt->frame_control) &&
	    !(rx->flags & IEEE80211_RX_BEACON_REPORTED)) {
		int sig = 0;

		if (ieee80211_hw_check(&rx->local->hw, SIGNAL_DBM) &&
		    !(status->flag & RX_FLAG_NO_SIGNAL_VAL))
			sig = status->signal;

		cfg80211_report_obss_beacon_khz(rx->local->hw.wiphy,
						rx->skb->data, rx->skb->len,
						ieee80211_rx_status_to_khz(status),
						sig);
		rx->flags |= IEEE80211_RX_BEACON_REPORTED;
	}

	if (ieee80211_drop_unencrypted_mgmt(rx))
		return RX_DROP_UNUSABLE;

	return RX_CONTINUE;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_action(struct ieee80211_rx_data *rx)
{
	struct ieee80211_local *local = rx->local;
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) rx->skb->data;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(rx->skb);
	int len = rx->skb->len;

	if (!ieee80211_is_action(mgmt->frame_control))
		return RX_CONTINUE;

	/* drop too small frames */
	if (len < IEEE80211_MIN_ACTION_SIZE)
		return RX_DROP_UNUSABLE;

	if (!rx->sta && mgmt->u.action.category != WLAN_CATEGORY_PUBLIC &&
	    mgmt->u.action.category != WLAN_CATEGORY_SELF_PROTECTED &&
	    mgmt->u.action.category != WLAN_CATEGORY_SPECTRUM_MGMT)
		return RX_DROP_UNUSABLE;

	switch (mgmt->u.action.category) {
	case WLAN_CATEGORY_HT:
		/* reject HT action frames from stations not supporting HT */
		if (!rx->sta->sta.ht_cap.ht_supported)
			goto invalid;

		if (sdata->vif.type != NL80211_IFTYPE_STATION &&
		    sdata->vif.type != NL80211_IFTYPE_MESH_POINT &&
		    sdata->vif.type != NL80211_IFTYPE_AP_VLAN &&
		    sdata->vif.type != NL80211_IFTYPE_AP &&
		    sdata->vif.type != NL80211_IFTYPE_ADHOC)
			break;

		/* verify action & smps_control/chanwidth are present */
		if (len < IEEE80211_MIN_ACTION_SIZE + 2)
			goto invalid;

		switch (mgmt->u.action.u.ht_smps.action) {
		case WLAN_HT_ACTION_SMPS: {
			struct ieee80211_supported_band *sband;
			enum ieee80211_smps_mode smps_mode;
			struct sta_opmode_info sta_opmode = {};

			if (sdata->vif.type != NL80211_IFTYPE_AP &&
			    sdata->vif.type != NL80211_IFTYPE_AP_VLAN)
				goto handled;

			/* convert to HT capability */
			switch (mgmt->u.action.u.ht_smps.smps_control) {
			case WLAN_HT_SMPS_CONTROL_DISABLED:
				smps_mode = IEEE80211_SMPS_OFF;
				break;
			case WLAN_HT_SMPS_CONTROL_STATIC:
				smps_mode = IEEE80211_SMPS_STATIC;
				break;
			case WLAN_HT_SMPS_CONTROL_DYNAMIC:
				smps_mode = IEEE80211_SMPS_DYNAMIC;
				break;
			default:
				goto invalid;
			}

			/* if no change do nothing */
			if (rx->sta->sta.smps_mode == smps_mode)
				goto handled;
			rx->sta->sta.smps_mode = smps_mode;
			sta_opmode.smps_mode =
				ieee80211_smps_mode_to_smps_mode(smps_mode);
			sta_opmode.changed = STA_OPMODE_SMPS_MODE_CHANGED;

			sband = rx->local->hw.wiphy->bands[status->band];

			rate_control_rate_update(local, sband, rx->sta,
						 IEEE80211_RC_SMPS_CHANGED);
			cfg80211_sta_opmode_change_notify(sdata->dev,
							  rx->sta->addr,
							  &sta_opmode,
							  GFP_ATOMIC);
			goto handled;
		}
		case WLAN_HT_ACTION_NOTIFY_CHANWIDTH: {
			struct ieee80211_supported_band *sband;
			u8 chanwidth = mgmt->u.action.u.ht_notify_cw.chanwidth;
			enum ieee80211_sta_rx_bandwidth max_bw, new_bw;
			struct sta_opmode_info sta_opmode = {};

			/* If it doesn't support 40 MHz it can't change ... */
			if (!(rx->sta->sta.ht_cap.cap &
					IEEE80211_HT_CAP_SUP_WIDTH_20_40))
				goto handled;

			if (chanwidth == IEEE80211_HT_CHANWIDTH_20MHZ)
				max_bw = IEEE80211_STA_RX_BW_20;
			else
				max_bw = ieee80211_sta_cap_rx_bw(rx->sta);

			/* set cur_max_bandwidth and recalc sta bw */
			rx->sta->cur_max_bandwidth = max_bw;
			new_bw = ieee80211_sta_cur_vht_bw(rx->sta);

			if (rx->sta->sta.bandwidth == new_bw)
				goto handled;

			rx->sta->sta.bandwidth = new_bw;
			sband = rx->local->hw.wiphy->bands[status->band];
			sta_opmode.bw =
				ieee80211_sta_rx_bw_to_chan_width(rx->sta);
			sta_opmode.changed = STA_OPMODE_MAX_BW_CHANGED;

			rate_control_rate_update(local, sband, rx->sta,
						 IEEE80211_RC_BW_CHANGED);
			cfg80211_sta_opmode_change_notify(sdata->dev,
							  rx->sta->addr,
							  &sta_opmode,
							  GFP_ATOMIC);
			goto handled;
		}
		default:
			goto invalid;
		}

		break;
	case WLAN_CATEGORY_PUBLIC:
		if (len < IEEE80211_MIN_ACTION_SIZE + 1)
			goto invalid;
		if (sdata->vif.type != NL80211_IFTYPE_STATION)
			break;
		if (!rx->sta)
			break;
		if (!ether_addr_equal(mgmt->bssid, sdata->u.mgd.bssid))
			break;
		if (mgmt->u.action.u.ext_chan_switch.action_code !=
				WLAN_PUB_ACTION_EXT_CHANSW_ANN)
			break;
		if (len < offsetof(struct ieee80211_mgmt,
				   u.action.u.ext_chan_switch.variable))
			goto invalid;
		goto queue;
	case WLAN_CATEGORY_VHT:
		if (sdata->vif.type != NL80211_IFTYPE_STATION &&
		    sdata->vif.type != NL80211_IFTYPE_MESH_POINT &&
		    sdata->vif.type != NL80211_IFTYPE_AP_VLAN &&
		    sdata->vif.type != NL80211_IFTYPE_AP &&
		    sdata->vif.type != NL80211_IFTYPE_ADHOC)
			break;

		/* verify action code is present */
		if (len < IEEE80211_MIN_ACTION_SIZE + 1)
			goto invalid;

		switch (mgmt->u.action.u.vht_opmode_notif.action_code) {
		case WLAN_VHT_ACTION_OPMODE_NOTIF: {
			/* verify opmode is present */
			if (len < IEEE80211_MIN_ACTION_SIZE + 2)
				goto invalid;
			goto queue;
		}
		case WLAN_VHT_ACTION_GROUPID_MGMT: {
			if (len < IEEE80211_MIN_ACTION_SIZE + 25)
				goto invalid;
			goto queue;
		}
		default:
			break;
		}
		break;
	case WLAN_CATEGORY_BACK:
		if (sdata->vif.type != NL80211_IFTYPE_STATION &&
		    sdata->vif.type != NL80211_IFTYPE_MESH_POINT &&
		    sdata->vif.type != NL80211_IFTYPE_AP_VLAN &&
		    sdata->vif.type != NL80211_IFTYPE_AP &&
		    sdata->vif.type != NL80211_IFTYPE_ADHOC)
			break;

		/* verify action_code is present */
		if (len < IEEE80211_MIN_ACTION_SIZE + 1)
			break;

		switch (mgmt->u.action.u.addba_req.action_code) {
		case WLAN_ACTION_ADDBA_REQ:
			if (len < (IEEE80211_MIN_ACTION_SIZE +
				   sizeof(mgmt->u.action.u.addba_req)))
				goto invalid;
			break;
		case WLAN_ACTION_ADDBA_RESP:
			if (len < (IEEE80211_MIN_ACTION_SIZE +
				   sizeof(mgmt->u.action.u.addba_resp)))
				goto invalid;
			break;
		case WLAN_ACTION_DELBA:
			if (len < (IEEE80211_MIN_ACTION_SIZE +
				   sizeof(mgmt->u.action.u.delba)))
				goto invalid;
			break;
		default:
			goto invalid;
		}

		goto queue;
	case WLAN_CATEGORY_SPECTRUM_MGMT:
		/* verify action_code is present */
		if (len < IEEE80211_MIN_ACTION_SIZE + 1)
			break;

		switch (mgmt->u.action.u.measurement.action_code) {
		case WLAN_ACTION_SPCT_MSR_REQ:
			if (status->band != NL80211_BAND_5GHZ)
				break;

			if (len < (IEEE80211_MIN_ACTION_SIZE +
				   sizeof(mgmt->u.action.u.measurement)))
				break;

			if (sdata->vif.type != NL80211_IFTYPE_STATION)
				break;

			ieee80211_process_measurement_req(sdata, mgmt, len);
			goto handled;
		case WLAN_ACTION_SPCT_CHL_SWITCH: {
			u8 *bssid;
			if (len < (IEEE80211_MIN_ACTION_SIZE +
				   sizeof(mgmt->u.action.u.chan_switch)))
				break;

			if (sdata->vif.type != NL80211_IFTYPE_STATION &&
			    sdata->vif.type != NL80211_IFTYPE_ADHOC &&
			    sdata->vif.type != NL80211_IFTYPE_MESH_POINT)
				break;

			if (sdata->vif.type == NL80211_IFTYPE_STATION)
				bssid = sdata->u.mgd.bssid;
			else if (sdata->vif.type == NL80211_IFTYPE_ADHOC)
				bssid = sdata->u.ibss.bssid;
			else if (sdata->vif.type == NL80211_IFTYPE_MESH_POINT)
				bssid = mgmt->sa;
			else
				break;

			if (!ether_addr_equal(mgmt->bssid, bssid))
				break;

			goto queue;
			}
		}
		break;
	case WLAN_CATEGORY_SELF_PROTECTED:
		if (len < (IEEE80211_MIN_ACTION_SIZE +
			   sizeof(mgmt->u.action.u.self_prot.action_code)))
			break;

		switch (mgmt->u.action.u.self_prot.action_code) {
		case WLAN_SP_MESH_PEERING_OPEN:
		case WLAN_SP_MESH_PEERING_CLOSE:
		case WLAN_SP_MESH_PEERING_CONFIRM:
			if (!ieee80211_vif_is_mesh(&sdata->vif))
				goto invalid;
			if (sdata->u.mesh.user_mpm)
				/* userspace handles this frame */
				break;
			goto queue;
		case WLAN_SP_MGK_INFORM:
		case WLAN_SP_MGK_ACK:
			if (!ieee80211_vif_is_mesh(&sdata->vif))
				goto invalid;
			break;
		}
		break;
	case WLAN_CATEGORY_MESH_ACTION:
		if (len < (IEEE80211_MIN_ACTION_SIZE +
			   sizeof(mgmt->u.action.u.mesh_action.action_code)))
			break;

		if (!ieee80211_vif_is_mesh(&sdata->vif))
			break;
		if (mesh_action_is_path_sel(mgmt) &&
		    !mesh_path_sel_is_hwmp(sdata))
			break;
		goto queue;
	}

	return RX_CONTINUE;

 invalid:
	status->rx_flags |= IEEE80211_RX_MALFORMED_ACTION_FRM;
	/* will return in the next handlers */
	return RX_CONTINUE;

 handled:
	if (rx->sta)
		rx->sta->rx_stats.packets++;
	dev_kfree_skb(rx->skb);
	return RX_QUEUED;

 queue:
	skb_queue_tail(&sdata->skb_queue, rx->skb);
	ieee80211_queue_work(&local->hw, &sdata->work);
	if (rx->sta)
		rx->sta->rx_stats.packets++;
	return RX_QUEUED;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_userspace_mgmt(struct ieee80211_rx_data *rx)
{
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(rx->skb);
	int sig = 0;

	/* skip known-bad action frames and return them in the next handler */
	if (status->rx_flags & IEEE80211_RX_MALFORMED_ACTION_FRM)
		return RX_CONTINUE;

	/*
	 * Getting here means the kernel doesn't know how to handle
	 * it, but maybe userspace does ... include returned frames
	 * so userspace can register for those to know whether ones
	 * it transmitted were processed or returned.
	 */

	if (ieee80211_hw_check(&rx->local->hw, SIGNAL_DBM) &&
	    !(status->flag & RX_FLAG_NO_SIGNAL_VAL))
		sig = status->signal;

	if (cfg80211_rx_mgmt_khz(&rx->sdata->wdev,
				 ieee80211_rx_status_to_khz(status), sig,
				 rx->skb->data, rx->skb->len, 0)) {
		if (rx->sta)
			rx->sta->rx_stats.packets++;
		dev_kfree_skb(rx->skb);
		return RX_QUEUED;
	}

	return RX_CONTINUE;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_action_post_userspace(struct ieee80211_rx_data *rx)
{
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) rx->skb->data;
	int len = rx->skb->len;

	if (!ieee80211_is_action(mgmt->frame_control))
		return RX_CONTINUE;

	switch (mgmt->u.action.category) {
	case WLAN_CATEGORY_SA_QUERY:
		if (len < (IEEE80211_MIN_ACTION_SIZE +
			   sizeof(mgmt->u.action.u.sa_query)))
			break;

		switch (mgmt->u.action.u.sa_query.action) {
		case WLAN_ACTION_SA_QUERY_REQUEST:
			if (sdata->vif.type != NL80211_IFTYPE_STATION)
				break;
			ieee80211_process_sa_query_req(sdata, mgmt, len);
			goto handled;
		}
		break;
	}

	return RX_CONTINUE;

 handled:
	if (rx->sta)
		rx->sta->rx_stats.packets++;
	dev_kfree_skb(rx->skb);
	return RX_QUEUED;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_action_return(struct ieee80211_rx_data *rx)
{
	struct ieee80211_local *local = rx->local;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) rx->skb->data;
	struct sk_buff *nskb;
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(rx->skb);

	if (!ieee80211_is_action(mgmt->frame_control))
		return RX_CONTINUE;

	/*
	 * For AP mode, hostapd is responsible for handling any action
	 * frames that we didn't handle, including returning unknown
	 * ones. For all other modes we will return them to the sender,
	 * setting the 0x80 bit in the action category, as required by
	 * 802.11-2012 9.24.4.
	 * Newer versions of hostapd shall also use the management frame
	 * registration mechanisms, but older ones still use cooked
	 * monitor interfaces so push all frames there.
	 */
	if (!(status->rx_flags & IEEE80211_RX_MALFORMED_ACTION_FRM) &&
	    (sdata->vif.type == NL80211_IFTYPE_AP ||
	     sdata->vif.type == NL80211_IFTYPE_AP_VLAN))
		return RX_DROP_MONITOR;

	if (is_multicast_ether_addr(mgmt->da))
		return RX_DROP_MONITOR;

	/* do not return rejected action frames */
	if (mgmt->u.action.category & 0x80)
		return RX_DROP_UNUSABLE;

	nskb = skb_copy_expand(rx->skb, local->hw.extra_tx_headroom, 0,
			       GFP_ATOMIC);
	if (nskb) {
		struct ieee80211_mgmt *nmgmt = (void *)nskb->data;

		nmgmt->u.action.category |= 0x80;
		memcpy(nmgmt->da, nmgmt->sa, ETH_ALEN);
		memcpy(nmgmt->sa, rx->sdata->vif.addr, ETH_ALEN);

		memset(nskb->cb, 0, sizeof(nskb->cb));

		if (rx->sdata->vif.type == NL80211_IFTYPE_P2P_DEVICE) {
			struct ieee80211_tx_info *info = IEEE80211_SKB_CB(nskb);

			info->flags = IEEE80211_TX_CTL_TX_OFFCHAN |
				      IEEE80211_TX_INTFL_OFFCHAN_TX_OK |
				      IEEE80211_TX_CTL_NO_CCK_RATE;
			if (ieee80211_hw_check(&local->hw, QUEUE_CONTROL))
				info->hw_queue =
					local->hw.offchannel_tx_hw_queue;
		}

		__ieee80211_tx_skb_tid_band(rx->sdata, nskb, 7,
					    status->band);
	}
	dev_kfree_skb(rx->skb);
	return RX_QUEUED;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_ext(struct ieee80211_rx_data *rx)
{
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_hdr *hdr = (void *)rx->skb->data;

	if (!ieee80211_is_ext(hdr->frame_control))
		return RX_CONTINUE;

	if (sdata->vif.type != NL80211_IFTYPE_STATION)
		return RX_DROP_MONITOR;

	/* for now only beacons are ext, so queue them */
	skb_queue_tail(&sdata->skb_queue, rx->skb);
	ieee80211_queue_work(&rx->local->hw, &sdata->work);
	if (rx->sta)
		rx->sta->rx_stats.packets++;

	return RX_QUEUED;
}

static ieee80211_rx_result debug_noinline
ieee80211_rx_h_mgmt(struct ieee80211_rx_data *rx)
{
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct ieee80211_mgmt *mgmt = (void *)rx->skb->data;
	__le16 stype;

	stype = mgmt->frame_control & cpu_to_le16(IEEE80211_FCTL_STYPE);

	if (!ieee80211_vif_is_mesh(&sdata->vif) &&
	    sdata->vif.type != NL80211_IFTYPE_ADHOC &&
	    sdata->vif.type != NL80211_IFTYPE_OCB &&
	    sdata->vif.type != NL80211_IFTYPE_STATION)
		return RX_DROP_MONITOR;

	switch (stype) {
	case cpu_to_le16(IEEE80211_STYPE_AUTH):
	case cpu_to_le16(IEEE80211_STYPE_BEACON):
	case cpu_to_le16(IEEE80211_STYPE_PROBE_RESP):
		/* process for all: mesh, mlme, ibss */
		break;
	case cpu_to_le16(IEEE80211_STYPE_DEAUTH):
		if (is_multicast_ether_addr(mgmt->da) &&
		    !is_broadcast_ether_addr(mgmt->da))
			return RX_DROP_MONITOR;

		/* process only for station/IBSS */
		if (sdata->vif.type != NL80211_IFTYPE_STATION &&
		    sdata->vif.type != NL80211_IFTYPE_ADHOC)
			return RX_DROP_MONITOR;
		break;
	case cpu_to_le16(IEEE80211_STYPE_ASSOC_RESP):
	case cpu_to_le16(IEEE80211_STYPE_REASSOC_RESP):
	case cpu_to_le16(IEEE80211_STYPE_DISASSOC):
		if (is_multicast_ether_addr(mgmt->da) &&
		    !is_broadcast_ether_addr(mgmt->da))
			return RX_DROP_MONITOR;

		/* process only for station */
		if (sdata->vif.type != NL80211_IFTYPE_STATION)
			return RX_DROP_MONITOR;
		break;
	case cpu_to_le16(IEEE80211_STYPE_PROBE_REQ):
		/* process only for ibss and mesh */
		if (sdata->vif.type != NL80211_IFTYPE_ADHOC &&
		    sdata->vif.type != NL80211_IFTYPE_MESH_POINT)
			return RX_DROP_MONITOR;
		break;
	default:
		return RX_DROP_MONITOR;
	}

	/* queue up frame and kick off work to process it */
	skb_queue_tail(&sdata->skb_queue, rx->skb);
	ieee80211_queue_work(&rx->local->hw, &sdata->work);
	if (rx->sta)
		rx->sta->rx_stats.packets++;

	return RX_QUEUED;
}

static void ieee80211_rx_cooked_monitor(struct ieee80211_rx_data *rx,
					struct ieee80211_rate *rate)
{
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_local *local = rx->local;
	struct sk_buff *skb = rx->skb, *skb2;
	struct net_device *prev_dev = NULL;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	int needed_headroom;

	/*
	 * If cooked monitor has been processed already, then
	 * don't do it again. If not, set the flag.
	 */
	if (rx->flags & IEEE80211_RX_CMNTR)
		goto out_free_skb;
	rx->flags |= IEEE80211_RX_CMNTR;

	/* If there are no cooked monitor interfaces, just free the SKB */
	if (!local->cooked_mntrs)
		goto out_free_skb;

	/* vendor data is long removed here */
	status->flag &= ~RX_FLAG_RADIOTAP_VENDOR_DATA;
	/* room for the radiotap header based on driver features */
	needed_headroom = ieee80211_rx_radiotap_hdrlen(local, status, skb);

	if (skb_headroom(skb) < needed_headroom &&
	    pskb_expand_head(skb, needed_headroom, 0, GFP_ATOMIC))
		goto out_free_skb;

	/* prepend radiotap information */
	ieee80211_add_rx_radiotap_header(local, skb, rate, needed_headroom,
					 false);

	skb_reset_mac_header(skb);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_802_2);

	list_for_each_entry_rcu(sdata, &local->interfaces, list) {
		if (!ieee80211_sdata_running(sdata))
			continue;

		if (sdata->vif.type != NL80211_IFTYPE_MONITOR ||
		    !(sdata->u.mntr.flags & MONITOR_FLAG_COOK_FRAMES))
			continue;

		if (prev_dev) {
			skb2 = skb_clone(skb, GFP_ATOMIC);
			if (skb2) {
				skb2->dev = prev_dev;
				netif_receive_skb(skb2);
			}
		}

		prev_dev = sdata->dev;
		dev_sw_netstats_rx_add(sdata->dev, skb->len);
	}

	if (prev_dev) {
		skb->dev = prev_dev;
		netif_receive_skb(skb);
		return;
	}

 out_free_skb:
	dev_kfree_skb(skb);
}

static void ieee80211_rx_handlers_result(struct ieee80211_rx_data *rx,
					 ieee80211_rx_result res)
{
	switch (res) {
	case RX_DROP_MONITOR:
		I802_DEBUG_INC(rx->sdata->local->rx_handlers_drop);
		if (rx->sta)
			rx->sta->rx_stats.dropped++;
		fallthrough;
	case RX_CONTINUE: {
		struct ieee80211_rate *rate = NULL;
		struct ieee80211_supported_band *sband;
		struct ieee80211_rx_status *status;

		status = IEEE80211_SKB_RXCB((rx->skb));

		sband = rx->local->hw.wiphy->bands[status->band];
		if (status->encoding == RX_ENC_LEGACY)
			rate = &sband->bitrates[status->rate_idx];

		ieee80211_rx_cooked_monitor(rx, rate);
		break;
		}
	case RX_DROP_UNUSABLE:
		I802_DEBUG_INC(rx->sdata->local->rx_handlers_drop);
		if (rx->sta)
			rx->sta->rx_stats.dropped++;
		dev_kfree_skb(rx->skb);
		break;
	case RX_QUEUED:
		I802_DEBUG_INC(rx->sdata->local->rx_handlers_queued);
		break;
	}
}

static void ieee80211_rx_handlers(struct ieee80211_rx_data *rx,
				  struct sk_buff_head *frames)
{
	ieee80211_rx_result res = RX_DROP_MONITOR;
	struct sk_buff *skb;

#define CALL_RXH(rxh)			\
	do {				\
		res = rxh(rx);		\
		if (res != RX_CONTINUE)	\
			goto rxh_next;  \
	} while (0)

	/* Lock here to avoid hitting all of the data used in the RX
	 * path (e.g. key data, station data, ...) concurrently when
	 * a frame is released from the reorder buffer due to timeout
	 * from the timer, potentially concurrently with RX from the
	 * driver.
	 */
	spin_lock_bh(&rx->local->rx_path_lock);

	while ((skb = __skb_dequeue(frames))) {
		/*
		 * all the other fields are valid across frames
		 * that belong to an aMPDU since they are on the
		 * same TID from the same station
		 */
		rx->skb = skb;

		CALL_RXH(ieee80211_rx_h_check_more_data);
		CALL_RXH(ieee80211_rx_h_uapsd_and_pspoll);
		CALL_RXH(ieee80211_rx_h_sta_process);
		CALL_RXH(ieee80211_rx_h_decrypt);
		CALL_RXH(ieee80211_rx_h_defragment);
		CALL_RXH(ieee80211_rx_h_michael_mic_verify);
		/* must be after MMIC verify so header is counted in MPDU mic */
#ifdef CONFIG_MAC80211_MESH
		if (ieee80211_vif_is_mesh(&rx->sdata->vif))
			CALL_RXH(ieee80211_rx_h_mesh_fwding);
#endif
		CALL_RXH(ieee80211_rx_h_amsdu);
		CALL_RXH(ieee80211_rx_h_data);

		/* special treatment -- needs the queue */
		res = ieee80211_rx_h_ctrl(rx, frames);
		if (res != RX_CONTINUE)
			goto rxh_next;

		CALL_RXH(ieee80211_rx_h_mgmt_check);
		CALL_RXH(ieee80211_rx_h_action);
		CALL_RXH(ieee80211_rx_h_userspace_mgmt);
		CALL_RXH(ieee80211_rx_h_action_post_userspace);
		CALL_RXH(ieee80211_rx_h_action_return);
		CALL_RXH(ieee80211_rx_h_ext);
		CALL_RXH(ieee80211_rx_h_mgmt);

 rxh_next:
		ieee80211_rx_handlers_result(rx, res);

#undef CALL_RXH
	}

	spin_unlock_bh(&rx->local->rx_path_lock);
}

static void ieee80211_invoke_rx_handlers(struct ieee80211_rx_data *rx)
{
	struct sk_buff_head reorder_release;
	ieee80211_rx_result res = RX_DROP_MONITOR;

	__skb_queue_head_init(&reorder_release);

#define CALL_RXH(rxh)			\
	do {				\
		res = rxh(rx);		\
		if (res != RX_CONTINUE)	\
			goto rxh_next;  \
	} while (0)

	CALL_RXH(ieee80211_rx_h_check_dup);
	CALL_RXH(ieee80211_rx_h_check);

	ieee80211_rx_reorder_ampdu(rx, &reorder_release);

	ieee80211_rx_handlers(rx, &reorder_release);
	return;

 rxh_next:
	ieee80211_rx_handlers_result(rx, res);

#undef CALL_RXH
}

/*
 * This function makes calls into the RX path, therefore
 * it has to be invoked under RCU read lock.
 */
void ieee80211_release_reorder_timeout(struct sta_info *sta, int tid)
{
	struct sk_buff_head frames;
	struct ieee80211_rx_data rx = {
		.sta = sta,
		.sdata = sta->sdata,
		.local = sta->local,
		/* This is OK -- must be QoS data frame */
		.security_idx = tid,
		.seqno_idx = tid,
	};
	struct tid_ampdu_rx *tid_agg_rx;

	tid_agg_rx = rcu_dereference(sta->ampdu_mlme.tid_rx[tid]);
	if (!tid_agg_rx)
		return;

	__skb_queue_head_init(&frames);

	spin_lock(&tid_agg_rx->reorder_lock);
	ieee80211_sta_reorder_release(sta->sdata, tid_agg_rx, &frames);
	spin_unlock(&tid_agg_rx->reorder_lock);

	if (!skb_queue_empty(&frames)) {
		struct ieee80211_event event = {
			.type = BA_FRAME_TIMEOUT,
			.u.ba.tid = tid,
			.u.ba.sta = &sta->sta,
		};
		drv_event_callback(rx.local, rx.sdata, &event);
	}

	ieee80211_rx_handlers(&rx, &frames);
}

void ieee80211_mark_rx_ba_filtered_frames(struct ieee80211_sta *pubsta, u8 tid,
					  u16 ssn, u64 filtered,
					  u16 received_mpdus)
{
	struct sta_info *sta;
	struct tid_ampdu_rx *tid_agg_rx;
	struct sk_buff_head frames;
	struct ieee80211_rx_data rx = {
		/* This is OK -- must be QoS data frame */
		.security_idx = tid,
		.seqno_idx = tid,
	};
	int i, diff;

	if (WARN_ON(!pubsta || tid >= IEEE80211_NUM_TIDS))
		return;

	__skb_queue_head_init(&frames);

	sta = container_of(pubsta, struct sta_info, sta);

	rx.sta = sta;
	rx.sdata = sta->sdata;
	rx.local = sta->local;

	rcu_read_lock();
	tid_agg_rx = rcu_dereference(sta->ampdu_mlme.tid_rx[tid]);
	if (!tid_agg_rx)
		goto out;

	spin_lock_bh(&tid_agg_rx->reorder_lock);

	if (received_mpdus >= IEEE80211_SN_MODULO >> 1) {
		int release;

		/* release all frames in the reorder buffer */
		release = (tid_agg_rx->head_seq_num + tid_agg_rx->buf_size) %
			   IEEE80211_SN_MODULO;
		ieee80211_release_reorder_frames(sta->sdata, tid_agg_rx,
						 release, &frames);
		/* update ssn to match received ssn */
		tid_agg_rx->head_seq_num = ssn;
	} else {
		ieee80211_release_reorder_frames(sta->sdata, tid_agg_rx, ssn,
						 &frames);
	}

	/* handle the case that received ssn is behind the mac ssn.
	 * it can be tid_agg_rx->buf_size behind and still be valid */
	diff = (tid_agg_rx->head_seq_num - ssn) & IEEE80211_SN_MASK;
	if (diff >= tid_agg_rx->buf_size) {
		tid_agg_rx->reorder_buf_filtered = 0;
		goto release;
	}
	filtered = filtered >> diff;
	ssn += diff;

	/* update bitmap */
	for (i = 0; i < tid_agg_rx->buf_size; i++) {
		int index = (ssn + i) % tid_agg_rx->buf_size;

		tid_agg_rx->reorder_buf_filtered &= ~BIT_ULL(index);
		if (filtered & BIT_ULL(i))
			tid_agg_rx->reorder_buf_filtered |= BIT_ULL(index);
	}

	/* now process also frames that the filter marking released */
	ieee80211_sta_reorder_release(sta->sdata, tid_agg_rx, &frames);

release:
	spin_unlock_bh(&tid_agg_rx->reorder_lock);

	ieee80211_rx_handlers(&rx, &frames);

 out:
	rcu_read_unlock();
}
EXPORT_SYMBOL(ieee80211_mark_rx_ba_filtered_frames);

/* main receive path */

static bool ieee80211_accept_frame(struct ieee80211_rx_data *rx)
{
	struct ieee80211_sub_if_data *sdata = rx->sdata;
	struct sk_buff *skb = rx->skb;
	struct ieee80211_hdr *hdr = (void *)skb->data;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	u8 *bssid = ieee80211_get_bssid(hdr, skb->len, sdata->vif.type);
	bool multicast = is_multicast_ether_addr(hdr->addr1) ||
			 ieee80211_is_s1g_beacon(hdr->frame_control);

	switch (sdata->vif.type) {
	case NL80211_IFTYPE_STATION:
		if (!bssid && !sdata->u.mgd.use_4addr)
			return false;
		if (ieee80211_is_robust_mgmt_frame(skb) && !rx->sta)
			return false;
		if (multicast)
			return true;
		return ether_addr_equal(sdata->vif.addr, hdr->addr1);
	case NL80211_IFTYPE_ADHOC:
		if (!bssid)
			return false;
		if (ether_addr_equal(sdata->vif.addr, hdr->addr2) ||
		    ether_addr_equal(sdata->u.ibss.bssid, hdr->addr2))
			return false;
		if (ieee80211_is_beacon(hdr->frame_control))
			return true;
		if (!ieee80211_bssid_match(bssid, sdata->u.ibss.bssid))
			return false;
		if (!multicast &&
		    !ether_addr_equal(sdata->vif.addr, hdr->addr1))
			return false;
		if (!rx->sta) {
			int rate_idx;
			if (status->encoding != RX_ENC_LEGACY)
				rate_idx = 0; /* TODO: HT/VHT rates */
			else
				rate_idx = status->rate_idx;
			ieee80211_ibss_rx_no_sta(sdata, bssid, hdr->addr2,
						 BIT(rate_idx));
		}
		return true;
	case NL80211_IFTYPE_OCB:
		if (!bssid)
			return false;
		if (!ieee80211_is_data_present(hdr->frame_control))
			return false;
		if (!is_broadcast_ether_addr(bssid))
			return false;
		if (!multicast &&
		    !ether_addr_equal(sdata->dev->dev_addr, hdr->addr1))
			return false;
		if (!rx->sta) {
			int rate_idx;
			if (status->encoding != RX_ENC_LEGACY)
				rate_idx = 0; /* TODO: HT rates */
			else
				rate_idx = status->rate_idx;
			ieee80211_ocb_rx_no_sta(sdata, bssid, hdr->addr2,
						BIT(rate_idx));
		}
		return true;
	case NL80211_IFTYPE_MESH_POINT:
		if (ether_addr_equal(sdata->vif.addr, hdr->addr2))
			return false;
		if (multicast)
			return true;
		return ether_addr_equal(sdata->vif.addr, hdr->addr1);
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_AP:
		if (!bssid)
			return ether_addr_equal(sdata->vif.addr, hdr->addr1);

		if (!ieee80211_bssid_match(bssid, sdata->vif.addr)) {
			/*
			 * Accept public action frames even when the
			 * BSSID doesn't match, this is used for P2P
			 * and location updates. Note that mac80211
			 * itself never looks at these frames.
			 */
			if (!multicast &&
			    !ether_addr_equal(sdata->vif.addr, hdr->addr1))
				return false;
			if (ieee80211_is_public_action(hdr, skb->len))
				return true;
			return ieee80211_is_beacon(hdr->frame_control);
		}

		if (!ieee80211_has_tods(hdr->frame_control)) {
			/* ignore data frames to TDLS-peers */
			if (ieee80211_is_data(hdr->frame_control))
				return false;
			/* ignore action frames to TDLS-peers */
			if (ieee80211_is_action(hdr->frame_control) &&
			    !is_broadcast_ether_addr(bssid) &&
			    !ether_addr_equal(bssid, hdr->addr1))
				return false;
		}

		/*
		 * 802.11-2016 Table 9-26 says that for data frames, A1 must be
		 * the BSSID - we've checked that already but may have accepted
		 * the wildcard (ff:ff:ff:ff:ff:ff).
		 *
		 * It also says:
		 *	The BSSID of the Data frame is determined as follows:
		 *	a) If the STA is contained within an AP or is associated
		 *	   with an AP, the BSSID is the address currently in use
		 *	   by the STA contained in the AP.
		 *
		 * So we should not accept data frames with an address that's
		 * multicast.
		 *
		 * Accepting it also opens a security problem because stations
		 * could encrypt it with the GTK and inject traffic that way.
		 */
		if (ieee80211_is_data(hdr->frame_control) && multicast)
			return false;

		return true;
	case NL80211_IFTYPE_P2P_DEVICE:
		return ieee80211_is_public_action(hdr, skb->len) ||
		       ieee80211_is_probe_req(hdr->frame_control) ||
		       ieee80211_is_probe_resp(hdr->frame_control) ||
		       ieee80211_is_beacon(hdr->frame_control);
	case NL80211_IFTYPE_NAN:
		/* Currently no frames on NAN interface are allowed */
		return false;
	default:
		break;
	}

	WARN_ON_ONCE(1);
	return false;
}

void ieee80211_check_fast_rx(struct sta_info *sta)
{
	struct ieee80211_sub_if_data *sdata = sta->sdata;
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_key *key;
	struct ieee80211_fast_rx fastrx = {
		.dev = sdata->dev,
		.vif_type = sdata->vif.type,
		.control_port_protocol = sdata->control_port_protocol,
	}, *old, *new = NULL;
	bool set_offload = false;
	bool assign = false;
	bool offload;

	/* use sparse to check that we don't return without updating */
	__acquire(check_fast_rx);

	BUILD_BUG_ON(sizeof(fastrx.rfc1042_hdr) != sizeof(rfc1042_header));
	BUILD_BUG_ON(sizeof(fastrx.rfc1042_hdr) != ETH_ALEN);
	ether_addr_copy(fastrx.rfc1042_hdr, rfc1042_header);
	ether_addr_copy(fastrx.vif_addr, sdata->vif.addr);

	fastrx.uses_rss = ieee80211_hw_check(&local->hw, USES_RSS);

	/* fast-rx doesn't do reordering */
	if (ieee80211_hw_check(&local->hw, AMPDU_AGGREGATION) &&
	    !ieee80211_hw_check(&local->hw, SUPPORTS_REORDERING_BUFFER))
		goto clear;

	switch (sdata->vif.type) {
	case NL80211_IFTYPE_STATION:
		if (sta->sta.tdls) {
			fastrx.da_offs = offsetof(struct ieee80211_hdr, addr1);
			fastrx.sa_offs = offsetof(struct ieee80211_hdr, addr2);
			fastrx.expected_ds_bits = 0;
		} else {
			fastrx.da_offs = offsetof(struct ieee80211_hdr, addr1);
			fastrx.sa_offs = offsetof(struct ieee80211_hdr, addr3);
			fastrx.expected_ds_bits =
				cpu_to_le16(IEEE80211_FCTL_FROMDS);
		}

		if (sdata->u.mgd.use_4addr && !sta->sta.tdls) {
			fastrx.expected_ds_bits |=
				cpu_to_le16(IEEE80211_FCTL_TODS);
			fastrx.da_offs = offsetof(struct ieee80211_hdr, addr3);
			fastrx.sa_offs = offsetof(struct ieee80211_hdr, addr4);
		}

		if (!sdata->u.mgd.powersave)
			break;

		/* software powersave is a huge mess, avoid all of it */
		if (ieee80211_hw_check(&local->hw, PS_NULLFUNC_STACK))
			goto clear;
		if (ieee80211_hw_check(&local->hw, SUPPORTS_PS) &&
		    !ieee80211_hw_check(&local->hw, SUPPORTS_DYNAMIC_PS))
			goto clear;
		break;
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_AP:
		/* parallel-rx requires this, at least with calls to
		 * ieee80211_sta_ps_transition()
		 */
		if (!ieee80211_hw_check(&local->hw, AP_LINK_PS))
			goto clear;
		fastrx.da_offs = offsetof(struct ieee80211_hdr, addr3);
		fastrx.sa_offs = offsetof(struct ieee80211_hdr, addr2);
		fastrx.expected_ds_bits = cpu_to_le16(IEEE80211_FCTL_TODS);

		fastrx.internal_forward =
			!(sdata->flags & IEEE80211_SDATA_DONT_BRIDGE_PACKETS) &&
			(sdata->vif.type != NL80211_IFTYPE_AP_VLAN ||
			 !sdata->u.vlan.sta);

		if (sdata->vif.type == NL80211_IFTYPE_AP_VLAN &&
		    sdata->u.vlan.sta) {
			fastrx.expected_ds_bits |=
				cpu_to_le16(IEEE80211_FCTL_FROMDS);
			fastrx.sa_offs = offsetof(struct ieee80211_hdr, addr4);
			fastrx.internal_forward = 0;
		}

		break;
	default:
		goto clear;
	}

	if (!test_sta_flag(sta, WLAN_STA_AUTHORIZED))
		goto clear;

	rcu_read_lock();
	key = rcu_dereference(sta->ptk[sta->ptk_idx]);
	if (!key)
		key = rcu_dereference(sdata->default_unicast_key);
	if (key) {
		switch (key->conf.cipher) {
		case WLAN_CIPHER_SUITE_TKIP:
			/* we don't want to deal with MMIC in fast-rx */
			goto clear_rcu;
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_CCMP_256:
		case WLAN_CIPHER_SUITE_GCMP:
		case WLAN_CIPHER_SUITE_GCMP_256:
			break;
		default:
			/* We also don't want to deal with
			 * WEP or cipher scheme.
			 */
			goto clear_rcu;
		}

		fastrx.key = true;
		fastrx.icv_len = key->conf.icv_len;
	}

	assign = true;
 clear_rcu:
	rcu_read_unlock();
 clear:
	__release(check_fast_rx);

	if (assign)
		new = kmemdup(&fastrx, sizeof(fastrx), GFP_KERNEL);

	offload = assign &&
		  (sdata->vif.offload_flags & IEEE80211_OFFLOAD_DECAP_ENABLED);

	if (offload)
		set_offload = !test_and_set_sta_flag(sta, WLAN_STA_DECAP_OFFLOAD);
	else
		set_offload = test_and_clear_sta_flag(sta, WLAN_STA_DECAP_OFFLOAD);

	if (set_offload)
		drv_sta_set_decap_offload(local, sdata, &sta->sta, assign);

	spin_lock_bh(&sta->lock);
	old = rcu_dereference_protected(sta->fast_rx, true);
	rcu_assign_pointer(sta->fast_rx, new);
	spin_unlock_bh(&sta->lock);

	if (old)
		kfree_rcu(old, rcu_head);
}

void ieee80211_clear_fast_rx(struct sta_info *sta)
{
	struct ieee80211_fast_rx *old;

	spin_lock_bh(&sta->lock);
	old = rcu_dereference_protected(sta->fast_rx, true);
	RCU_INIT_POINTER(sta->fast_rx, NULL);
	spin_unlock_bh(&sta->lock);

	if (old)
		kfree_rcu(old, rcu_head);
}

void __ieee80211_check_fast_rx_iface(struct ieee80211_sub_if_data *sdata)
{
	struct ieee80211_local *local = sdata->local;
	struct sta_info *sta;

	lockdep_assert_held(&local->sta_mtx);

	list_for_each_entry(sta, &local->sta_list, list) {
		if (sdata != sta->sdata &&
		    (!sta->sdata->bss || sta->sdata->bss != sdata->bss))
			continue;
		ieee80211_check_fast_rx(sta);
	}
}

void ieee80211_check_fast_rx_iface(struct ieee80211_sub_if_data *sdata)
{
	struct ieee80211_local *local = sdata->local;

	mutex_lock(&local->sta_mtx);
	__ieee80211_check_fast_rx_iface(sdata);
	mutex_unlock(&local->sta_mtx);
}

static void ieee80211_rx_8023(struct ieee80211_rx_data *rx,
			      struct ieee80211_fast_rx *fast_rx,
			      int orig_len)
{
	struct ieee80211_sta_rx_stats *stats;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(rx->skb);
	struct sta_info *sta = rx->sta;
	struct sk_buff *skb = rx->skb;
	void *sa = skb->data + ETH_ALEN;
	void *da = skb->data;

	stats = &sta->rx_stats;
	if (fast_rx->uses_rss)
		stats = this_cpu_ptr(sta->pcpu_rx_stats);

	/* statistics part of ieee80211_rx_h_sta_process() */
	if (!(status->flag & RX_FLAG_NO_SIGNAL_VAL)) {
		stats->last_signal = status->signal;
		if (!fast_rx->uses_rss)
			ewma_signal_add(&sta->rx_stats_avg.signal,
					-status->signal);
	}

	if (status->chains) {
		int i;

		stats->chains = status->chains;
		for (i = 0; i < ARRAY_SIZE(status->chain_signal); i++) {
			int signal = status->chain_signal[i];

			if (!(status->chains & BIT(i)))
				continue;

			stats->chain_signal_last[i] = signal;
			if (!fast_rx->uses_rss)
				ewma_signal_add(&sta->rx_stats_avg.chain_signal[i],
						-signal);
		}
	}
	/* end of statistics */

	stats->last_rx = jiffies;
	stats->last_rate = sta_stats_encode_rate(status);

	stats->fragments++;
	stats->packets++;

	skb->dev = fast_rx->dev;

	dev_sw_netstats_rx_add(fast_rx->dev, skb->len);

	/* The seqno index has the same property as needed
	 * for the rx_msdu field, i.e. it is IEEE80211_NUM_TIDS
	 * for non-QoS-data frames. Here we know it's a data
	 * frame, so count MSDUs.
	 */
	u64_stats_update_begin(&stats->syncp);
	stats->msdu[rx->seqno_idx]++;
	stats->bytes += orig_len;
	u64_stats_update_end(&stats->syncp);

	if (fast_rx->internal_forward) {
		struct sk_buff *xmit_skb = NULL;
		if (is_multicast_ether_addr(da)) {
			xmit_skb = skb_copy(skb, GFP_ATOMIC);
		} else if (!ether_addr_equal(da, sa) &&
			   sta_info_get(rx->sdata, da)) {
			xmit_skb = skb;
			skb = NULL;
		}

		if (xmit_skb) {
			/*
			 * Send to wireless media and increase priority by 256
			 * to keep the received priority instead of
			 * reclassifying the frame (see cfg80211_classify8021d).
			 */
			xmit_skb->priority += 256;
			xmit_skb->protocol = htons(ETH_P_802_3);
			skb_reset_network_header(xmit_skb);
			skb_reset_mac_header(xmit_skb);
			dev_queue_xmit(xmit_skb);
		}

		if (!skb)
			return;
	}

	/* deliver to local stack */
	skb->protocol = eth_type_trans(skb, fast_rx->dev);
	memset(skb->cb, 0, sizeof(skb->cb));
	if (rx->list)
		list_add_tail(&skb->list, rx->list);
	else
		netif_receive_skb(skb);

}

static bool ieee80211_invoke_fast_rx(struct ieee80211_rx_data *rx,
				     struct ieee80211_fast_rx *fast_rx)
{
	struct sk_buff *skb = rx->skb;
	struct ieee80211_hdr *hdr = (void *)skb->data;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct sta_info *sta = rx->sta;
	int orig_len = skb->len;
	int hdrlen = ieee80211_hdrlen(hdr->frame_control);
	int snap_offs = hdrlen;
	struct {
		u8 snap[sizeof(rfc1042_header)];
		__be16 proto;
	} *payload __aligned(2);
	struct {
		u8 da[ETH_ALEN];
		u8 sa[ETH_ALEN];
	} addrs __aligned(2);
	struct ieee80211_sta_rx_stats *stats = &sta->rx_stats;

	/* for parallel-rx, we need to have DUP_VALIDATED, otherwise we write
	 * to a common data structure; drivers can implement that per queue
	 * but we don't have that information in mac80211
	 */
	if (!(status->flag & RX_FLAG_DUP_VALIDATED))
		return false;

#define FAST_RX_CRYPT_FLAGS	(RX_FLAG_PN_VALIDATED | RX_FLAG_DECRYPTED)

	/* If using encryption, we also need to have:
	 *  - PN_VALIDATED: similar, but the implementation is tricky
	 *  - DECRYPTED: necessary for PN_VALIDATED
	 */
	if (fast_rx->key &&
	    (status->flag & FAST_RX_CRYPT_FLAGS) != FAST_RX_CRYPT_FLAGS)
		return false;

	if (unlikely(!ieee80211_is_data_present(hdr->frame_control)))
		return false;

	if (unlikely(ieee80211_is_frag(hdr)))
		return false;

	/* Since our interface address cannot be multicast, this
	 * implicitly also rejects multicast frames without the
	 * explicit check.
	 *
	 * We shouldn't get any *data* frames not addressed to us
	 * (AP mode will accept multicast *management* frames), but
	 * punting here will make it go through the full checks in
	 * ieee80211_accept_frame().
	 */
	if (!ether_addr_equal(fast_rx->vif_addr, hdr->addr1))
		return false;

	if ((hdr->frame_control & cpu_to_le16(IEEE80211_FCTL_FROMDS |
					      IEEE80211_FCTL_TODS)) !=
	    fast_rx->expected_ds_bits)
		return false;

	/* assign the key to drop unencrypted frames (later)
	 * and strip the IV/MIC if necessary
	 */
	if (fast_rx->key && !(status->flag & RX_FLAG_IV_STRIPPED)) {
		/* GCMP header length is the same */
		snap_offs += IEEE80211_CCMP_HDR_LEN;
	}

	if (!(status->rx_flags & IEEE80211_RX_AMSDU)) {
		if (!pskb_may_pull(skb, snap_offs + sizeof(*payload)))
			goto drop;

		payload = (void *)(skb->data + snap_offs);

		if (!ether_addr_equal(payload->snap, fast_rx->rfc1042_hdr))
			return false;

		/* Don't handle these here since they require special code.
		 * Accept AARP and IPX even though they should come with a
		 * bridge-tunnel header - but if we get them this way then
		 * there's little point in discarding them.
		 */
		if (unlikely(payload->proto == cpu_to_be16(ETH_P_TDLS) ||
			     payload->proto == fast_rx->control_port_protocol))
			return false;
	}

	/* after this point, don't punt to the slowpath! */

	if (rx->key && !(status->flag & RX_FLAG_MIC_STRIPPED) &&
	    pskb_trim(skb, skb->len - fast_rx->icv_len))
		goto drop;

	if (rx->key && !ieee80211_has_protected(hdr->frame_control))
		goto drop;

	if (status->rx_flags & IEEE80211_RX_AMSDU) {
		if (__ieee80211_rx_h_amsdu(rx, snap_offs - hdrlen) !=
		    RX_QUEUED)
			goto drop;

		return true;
	}

	/* do the header conversion - first grab the addresses */
	ether_addr_copy(addrs.da, skb->data + fast_rx->da_offs);
	ether_addr_copy(addrs.sa, skb->data + fast_rx->sa_offs);
	/* remove the SNAP but leave the ethertype */
	skb_pull(skb, snap_offs + sizeof(rfc1042_header));
	/* push the addresses in front */
	memcpy(skb_push(skb, sizeof(addrs)), &addrs, sizeof(addrs));

	ieee80211_rx_8023(rx, fast_rx, orig_len);

	return true;
 drop:
	dev_kfree_skb(skb);
	if (fast_rx->uses_rss)
		stats = this_cpu_ptr(sta->pcpu_rx_stats);

	stats->dropped++;
	return true;
}

/*
 * This function returns whether or not the SKB
 * was destined for RX processing or not, which,
 * if consume is true, is equivalent to whether
 * or not the skb was consumed.
 */
static bool ieee80211_prepare_and_rx_handle(struct ieee80211_rx_data *rx,
					    struct sk_buff *skb, bool consume)
{
	struct ieee80211_local *local = rx->local;
	struct ieee80211_sub_if_data *sdata = rx->sdata;

	rx->skb = skb;

	/* See if we can do fast-rx; if we have to copy we already lost,
	 * so punt in that case. We should never have to deliver a data
	 * frame to multiple interfaces anyway.
	 *
	 * We skip the ieee80211_accept_frame() call and do the necessary
	 * checking inside ieee80211_invoke_fast_rx().
	 */
	if (consume && rx->sta) {
		struct ieee80211_fast_rx *fast_rx;

		fast_rx = rcu_dereference(rx->sta->fast_rx);
		if (fast_rx && ieee80211_invoke_fast_rx(rx, fast_rx))
			return true;
	}

	if (!ieee80211_accept_frame(rx))
		return false;

	if (!consume) {
		skb = skb_copy(skb, GFP_ATOMIC);
		if (!skb) {
			if (net_ratelimit())
				wiphy_debug(local->hw.wiphy,
					"failed to copy skb for %s\n",
					sdata->name);
			return true;
		}

		rx->skb = skb;
	}

	ieee80211_invoke_rx_handlers(rx);
	return true;
}

static void __ieee80211_rx_handle_8023(struct ieee80211_hw *hw,
				       struct ieee80211_sta *pubsta,
				       struct sk_buff *skb,
				       struct list_head *list)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_fast_rx *fast_rx;
	struct ieee80211_rx_data rx;

	memset(&rx, 0, sizeof(rx));
	rx.skb = skb;
	rx.local = local;
	rx.list = list;

	I802_DEBUG_INC(local->dot11ReceivedFragmentCount);

	/* drop frame if too short for header */
	if (skb->len < sizeof(struct ethhdr))
		goto drop;

	if (!pubsta)
		goto drop;

	rx.sta = container_of(pubsta, struct sta_info, sta);
	rx.sdata = rx.sta->sdata;

	fast_rx = rcu_dereference(rx.sta->fast_rx);
	if (!fast_rx)
		goto drop;

	ieee80211_rx_8023(&rx, fast_rx, skb->len);
	return;

drop:
	dev_kfree_skb(skb);
}

/*
 * This is the actual Rx frames handler. as it belongs to Rx path it must
 * be called with rcu_read_lock protection.
 */
static void __ieee80211_rx_handle_packet(struct ieee80211_hw *hw,
					 struct ieee80211_sta *pubsta,
					 struct sk_buff *skb,
					 struct list_head *list)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_hdr *hdr;
	__le16 fc;
	struct ieee80211_rx_data rx;
	struct ieee80211_sub_if_data *prev;
	struct rhlist_head *tmp;
	int err = 0;

	fc = ((struct ieee80211_hdr *)skb->data)->frame_control;
	memset(&rx, 0, sizeof(rx));
	rx.skb = skb;
	rx.local = local;
	rx.list = list;

	if (ieee80211_is_data(fc) || ieee80211_is_mgmt(fc))
		I802_DEBUG_INC(local->dot11ReceivedFragmentCount);

	if (ieee80211_is_mgmt(fc)) {
		/* drop frame if too short for header */
		if (skb->len < ieee80211_hdrlen(fc))
			err = -ENOBUFS;
		else
			err = skb_linearize(skb);
	} else {
		err = !pskb_may_pull(skb, ieee80211_hdrlen(fc));
	}

	if (err) {
		dev_kfree_skb(skb);
		return;
	}

	hdr = (struct ieee80211_hdr *)skb->data;
	ieee80211_parse_qos(&rx);
	ieee80211_verify_alignment(&rx);

	if (unlikely(ieee80211_is_probe_resp(hdr->frame_control) ||
		     ieee80211_is_beacon(hdr->frame_control) ||
		     ieee80211_is_s1g_beacon(hdr->frame_control)))
		ieee80211_scan_rx(local, skb);

	if (ieee80211_is_data(fc)) {
		struct sta_info *sta, *prev_sta;

		if (pubsta) {
			rx.sta = container_of(pubsta, struct sta_info, sta);
			rx.sdata = rx.sta->sdata;
			if (ieee80211_prepare_and_rx_handle(&rx, skb, true))
				return;
			goto out;
		}

		prev_sta = NULL;

		for_each_sta_info(local, hdr->addr2, sta, tmp) {
			if (!prev_sta) {
				prev_sta = sta;
				continue;
			}

			rx.sta = prev_sta;
			rx.sdata = prev_sta->sdata;
			ieee80211_prepare_and_rx_handle(&rx, skb, false);

			prev_sta = sta;
		}

		if (prev_sta) {
			rx.sta = prev_sta;
			rx.sdata = prev_sta->sdata;

			if (ieee80211_prepare_and_rx_handle(&rx, skb, true))
				return;
			goto out;
		}
	}

	prev = NULL;

	list_for_each_entry_rcu(sdata, &local->interfaces, list) {
		if (!ieee80211_sdata_running(sdata))
			continue;

		if (sdata->vif.type == NL80211_IFTYPE_MONITOR ||
		    sdata->vif.type == NL80211_IFTYPE_AP_VLAN)
			continue;

		/*
		 * frame is destined for this interface, but if it's
		 * not also for the previous one we handle that after
		 * the loop to avoid copying the SKB once too much
		 */

		if (!prev) {
			prev = sdata;
			continue;
		}

		rx.sta = sta_info_get_bss(prev, hdr->addr2);
		rx.sdata = prev;
		ieee80211_prepare_and_rx_handle(&rx, skb, false);

		prev = sdata;
	}

	if (prev) {
		rx.sta = sta_info_get_bss(prev, hdr->addr2);
		rx.sdata = prev;

		if (ieee80211_prepare_and_rx_handle(&rx, skb, true))
			return;
	}

 out:
	dev_kfree_skb(skb);
}

/*
 * This is the receive path handler. It is called by a low level driver when an
 * 802.11 MPDU is received from the hardware.
 */
void ieee80211_rx_list(struct ieee80211_hw *hw, struct ieee80211_sta *pubsta,
		       struct sk_buff *skb, struct list_head *list)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_rate *rate = NULL;
	struct ieee80211_supported_band *sband;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);

	WARN_ON_ONCE(softirq_count() == 0);

	if (WARN_ON(status->band >= NUM_NL80211_BANDS))
		goto drop;

	sband = local->hw.wiphy->bands[status->band];
	if (WARN_ON(!sband))
		goto drop;

	/*
	 * If we're suspending, it is possible although not too likely
	 * that we'd be receiving frames after having already partially
	 * quiesced the stack. We can't process such frames then since
	 * that might, for example, cause stations to be added or other
	 * driver callbacks be invoked.
	 */
	if (unlikely(local->quiescing || local->suspended))
		goto drop;

	/* We might be during a HW reconfig, prevent Rx for the same reason */
	if (unlikely(local->in_reconfig))
		goto drop;

	/*
	 * The same happens when we're not even started,
	 * but that's worth a warning.
	 */
	if (WARN_ON(!local->started))
		goto drop;

	if (likely(!(status->flag & RX_FLAG_FAILED_PLCP_CRC))) {
		/*
		 * Validate the rate, unless a PLCP error means that
		 * we probably can't have a valid rate here anyway.
		 */

		switch (status->encoding) {
		case RX_ENC_HT:
			/*
			 * rate_idx is MCS index, which can be [0-76]
			 * as documented on:
			 *
			 * https://wireless.wiki.kernel.org/en/developers/Documentation/ieee80211/802.11n
			 *
			 * Anything else would be some sort of driver or
			 * hardware error. The driver should catch hardware
			 * errors.
			 */
			if (WARN(status->rate_idx > 76,
				 "Rate marked as an HT rate but passed "
				 "status->rate_idx is not "
				 "an MCS index [0-76]: %d (0x%02x)\n",
				 status->rate_idx,
				 status->rate_idx))
				goto drop;
			break;
		case RX_ENC_VHT:
			if (WARN_ONCE(status->rate_idx > 9 ||
				      !status->nss ||
				      status->nss > 8,
				      "Rate marked as a VHT rate but data is invalid: MCS: %d, NSS: %d\n",
				      status->rate_idx, status->nss))
				goto drop;
			break;
		case RX_ENC_HE:
			if (WARN_ONCE(status->rate_idx > 11 ||
				      !status->nss ||
				      status->nss > 8,
				      "Rate marked as an HE rate but data is invalid: MCS: %d, NSS: %d\n",
				      status->rate_idx, status->nss))
				goto drop;
			break;
		default:
			WARN_ON_ONCE(1);
			fallthrough;
		case RX_ENC_LEGACY:
			if (WARN_ON(status->rate_idx >= sband->n_bitrates))
				goto drop;
			rate = &sband->bitrates[status->rate_idx];
		}
	}

	status->rx_flags = 0;

	kcov_remote_start_common(skb_get_kcov_handle(skb));

	/*
	 * Frames with failed FCS/PLCP checksum are not returned,
	 * all other frames are returned without radiotap header
	 * if it was previously present.
	 * Also, frames with less than 16 bytes are dropped.
	 */
	if (!(status->flag & RX_FLAG_8023))
		skb = ieee80211_rx_monitor(local, skb, rate);
	if (skb) {
		ieee80211_tpt_led_trig_rx(local,
					  ((struct ieee80211_hdr *)skb->data)->frame_control,
					  skb->len);

		if (status->flag & RX_FLAG_8023)
			__ieee80211_rx_handle_8023(hw, pubsta, skb, list);
		else
			__ieee80211_rx_handle_packet(hw, pubsta, skb, list);
	}

	kcov_remote_stop();
	return;
 drop:
	kfree_skb(skb);
}
EXPORT_SYMBOL(ieee80211_rx_list);

void ieee80211_rx_napi(struct ieee80211_hw *hw, struct ieee80211_sta *pubsta,
		       struct sk_buff *skb, struct napi_struct *napi)
{
	struct sk_buff *tmp;
	LIST_HEAD(list);


	/*
	 * key references and virtual interfaces are protected using RCU
	 * and this requires that we are in a read-side RCU section during
	 * receive processing
	 */
	rcu_read_lock();
	ieee80211_rx_list(hw, pubsta, skb, &list);
	rcu_read_unlock();

	if (!napi) {
		netif_receive_skb_list(&list);
		return;
	}

	list_for_each_entry_safe(skb, tmp, &list, list) {
		skb_list_del_init(skb);
		napi_gro_receive(napi, skb);
	}
}
EXPORT_SYMBOL(ieee80211_rx_napi);

/* This is a version of the rx handler that can be called from hard irq
 * context. Post the skb on the queue and schedule the tasklet */
void ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ieee80211_local *local = hw_to_local(hw);

	BUILD_BUG_ON(sizeof(struct ieee80211_rx_status) > sizeof(skb->cb));

	skb->pkt_type = IEEE80211_RX_MSG;
	skb_queue_tail(&local->skb_queue, skb);
	tasklet_schedule(&local->tasklet);
}
EXPORT_SYMBOL(ieee80211_rx_irqsafe);