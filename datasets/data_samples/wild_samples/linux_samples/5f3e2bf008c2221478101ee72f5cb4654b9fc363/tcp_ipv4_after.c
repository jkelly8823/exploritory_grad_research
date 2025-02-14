	for_each_possible_cpu(cpu) {
		struct sock *sk;

		res = inet_ctl_sock_create(&sk, PF_INET, SOCK_RAW,
					   IPPROTO_TCP, net);
		if (res)
			goto fail;
		sock_set_flag(sk, SOCK_USE_WRITE_QUEUE);

		/* Please enforce IP_DF and IPID==0 for RST and
		 * ACK sent in SYN-RECV and TIME-WAIT state.
		 */
		inet_sk(sk)->pmtudisc = IP_PMTUDISC_DO;

		*per_cpu_ptr(net->ipv4.tcp_sk, cpu) = sk;
	}

	net->ipv4.sysctl_tcp_ecn = 2;
	net->ipv4.sysctl_tcp_ecn_fallback = 1;

	net->ipv4.sysctl_tcp_base_mss = TCP_BASE_MSS;
	net->ipv4.sysctl_tcp_min_snd_mss = TCP_MIN_SND_MSS;
	net->ipv4.sysctl_tcp_probe_threshold = TCP_PROBE_THRESHOLD;
	net->ipv4.sysctl_tcp_probe_interval = TCP_PROBE_INTERVAL;

	net->ipv4.sysctl_tcp_keepalive_time = TCP_KEEPALIVE_TIME;
	net->ipv4.sysctl_tcp_keepalive_probes = TCP_KEEPALIVE_PROBES;
	net->ipv4.sysctl_tcp_keepalive_intvl = TCP_KEEPALIVE_INTVL;

	net->ipv4.sysctl_tcp_syn_retries = TCP_SYN_RETRIES;
	net->ipv4.sysctl_tcp_synack_retries = TCP_SYNACK_RETRIES;
	net->ipv4.sysctl_tcp_syncookies = 1;
	net->ipv4.sysctl_tcp_reordering = TCP_FASTRETRANS_THRESH;
	net->ipv4.sysctl_tcp_retries1 = TCP_RETR1;
	net->ipv4.sysctl_tcp_retries2 = TCP_RETR2;
	net->ipv4.sysctl_tcp_orphan_retries = 0;
	net->ipv4.sysctl_tcp_fin_timeout = TCP_FIN_TIMEOUT;
	net->ipv4.sysctl_tcp_notsent_lowat = UINT_MAX;
	net->ipv4.sysctl_tcp_tw_reuse = 2;

	cnt = tcp_hashinfo.ehash_mask + 1;
	net->ipv4.tcp_death_row.sysctl_max_tw_buckets = cnt / 2;
	net->ipv4.tcp_death_row.hashinfo = &tcp_hashinfo;

	net->ipv4.sysctl_max_syn_backlog = max(128, cnt / 256);
	net->ipv4.sysctl_tcp_sack = 1;
	net->ipv4.sysctl_tcp_window_scaling = 1;
	net->ipv4.sysctl_tcp_timestamps = 1;
	net->ipv4.sysctl_tcp_early_retrans = 3;
	net->ipv4.sysctl_tcp_recovery = TCP_RACK_LOSS_DETECTION;
	net->ipv4.sysctl_tcp_slow_start_after_idle = 1; /* By default, RFC2861 behavior.  */
	net->ipv4.sysctl_tcp_retrans_collapse = 1;
	net->ipv4.sysctl_tcp_max_reordering = 300;
	net->ipv4.sysctl_tcp_dsack = 1;
	net->ipv4.sysctl_tcp_app_win = 31;
	net->ipv4.sysctl_tcp_adv_win_scale = 1;
	net->ipv4.sysctl_tcp_frto = 2;
	net->ipv4.sysctl_tcp_moderate_rcvbuf = 1;
	/* This limits the percentage of the congestion window which we
	 * will allow a single TSO frame to consume.  Building TSO frames
	 * which are too large can cause TCP streams to be bursty.
	 */
	net->ipv4.sysctl_tcp_tso_win_divisor = 3;
	/* Default TSQ limit of 16 TSO segments */
	net->ipv4.sysctl_tcp_limit_output_bytes = 16 * 65536;
	/* rfc5961 challenge ack rate limiting */
	net->ipv4.sysctl_tcp_challenge_ack_limit = 1000;
	net->ipv4.sysctl_tcp_min_tso_segs = 2;
	net->ipv4.sysctl_tcp_min_rtt_wlen = 300;
	net->ipv4.sysctl_tcp_autocorking = 1;
	net->ipv4.sysctl_tcp_invalid_ratelimit = HZ/2;
	net->ipv4.sysctl_tcp_pacing_ss_ratio = 200;
	net->ipv4.sysctl_tcp_pacing_ca_ratio = 120;
	if (net != &init_net) {
		memcpy(net->ipv4.sysctl_tcp_rmem,
		       init_net.ipv4.sysctl_tcp_rmem,
		       sizeof(init_net.ipv4.sysctl_tcp_rmem));
		memcpy(net->ipv4.sysctl_tcp_wmem,
		       init_net.ipv4.sysctl_tcp_wmem,
		       sizeof(init_net.ipv4.sysctl_tcp_wmem));
	}