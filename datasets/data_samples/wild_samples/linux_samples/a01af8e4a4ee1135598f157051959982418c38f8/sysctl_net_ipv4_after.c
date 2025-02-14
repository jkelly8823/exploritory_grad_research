/*
 * sysctl_net_ipv4.c: sysctl interface to net IPV4 subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/ipv4 directory entry (empty =) ). [MS]
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/igmp.h>
#include <linux/inetdevice.h>
#include <linux/seqlock.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <net/snmp.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/cipso_ipv4.h>
#include <net/inet_frag.h>

static int zero;
static int tcp_retr1_max = 255;
static int ip_local_port_range_min[] = { 1, 1 };
static int ip_local_port_range_max[] = { 65535, 65535 };
static int tcp_adv_win_scale_min = -31;
static int tcp_adv_win_scale_max = 31;

/* Update system visible IP port range */
static void set_local_port_range(int range[2])
{
	write_seqlock(&sysctl_local_ports.lock);
	sysctl_local_ports.range[0] = range[0];
	sysctl_local_ports.range[1] = range[1];
	write_sequnlock(&sysctl_local_ports.lock);
}
	{
		.procname	= "tcp_adv_win_scale",
		.data		= &sysctl_tcp_adv_win_scale,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &tcp_adv_win_scale_min,
		.extra2		= &tcp_adv_win_scale_max,
	},
	{
		.procname	= "tcp_tw_reuse",
		.data		= &sysctl_tcp_tw_reuse,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "tcp_frto",
		.data		= &sysctl_tcp_frto,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "tcp_frto_response",
		.data		= &sysctl_tcp_frto_response,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "tcp_low_latency",
		.data		= &sysctl_tcp_low_latency,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "tcp_no_metrics_save",
		.data		= &sysctl_tcp_nometrics_save,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_moderate_rcvbuf",
		.data		= &sysctl_tcp_moderate_rcvbuf,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_tso_win_divisor",
		.data		= &sysctl_tcp_tso_win_divisor,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_congestion_control",
		.mode		= 0644,
		.maxlen		= TCP_CA_NAME_MAX,
		.proc_handler	= proc_tcp_congestion_control,
	},
	{
		.procname	= "tcp_abc",
		.data		= &sysctl_tcp_abc,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_mtu_probing",
		.data		= &sysctl_tcp_mtu_probing,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_base_mss",
		.data		= &sysctl_tcp_base_mss,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_workaround_signed_windows",
		.data		= &sysctl_tcp_workaround_signed_windows,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
#ifdef CONFIG_NET_DMA
	{
		.procname	= "tcp_dma_copybreak",
		.data		= &sysctl_tcp_dma_copybreak,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
#endif
	{
		.procname	= "tcp_slow_start_after_idle",
		.data		= &sysctl_tcp_slow_start_after_idle,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
#ifdef CONFIG_NETLABEL
	{
		.procname	= "cipso_cache_enable",
		.data		= &cipso_v4_cache_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "cipso_cache_bucket_size",
		.data		= &cipso_v4_cache_bucketsize,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "cipso_rbm_optfmt",
		.data		= &cipso_v4_rbm_optfmt,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "cipso_rbm_strictvalid",
		.data		= &cipso_v4_rbm_strictvalid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif /* CONFIG_NETLABEL */
	{
		.procname	= "tcp_available_congestion_control",
		.maxlen		= TCP_CA_BUF_MAX,
		.mode		= 0444,
		.proc_handler   = proc_tcp_available_congestion_control,
	},
	{
		.procname	= "tcp_allowed_congestion_control",
		.maxlen		= TCP_CA_BUF_MAX,
		.mode		= 0644,
		.proc_handler   = proc_allowed_congestion_control,
	},
	{
		.procname	= "tcp_max_ssthresh",
		.data		= &sysctl_tcp_max_ssthresh,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcp_cookie_size",
		.data		= &sysctl_tcp_cookie_size,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname       = "tcp_thin_linear_timeouts",
		.data           = &sysctl_tcp_thin_linear_timeouts,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec
	},
        {
		.procname       = "tcp_thin_dupack",
		.data           = &sysctl_tcp_thin_dupack,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec
	},
	{
		.procname	= "udp_mem",
		.data		= &sysctl_udp_mem,
		.maxlen		= sizeof(sysctl_udp_mem),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
	},
	{
		.procname	= "udp_rmem_min",
		.data		= &sysctl_udp_rmem_min,
		.maxlen		= sizeof(sysctl_udp_rmem_min),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero
	},
	{
		.procname	= "udp_wmem_min",
		.data		= &sysctl_udp_wmem_min,
		.maxlen		= sizeof(sysctl_udp_wmem_min),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero
	},
	{ }
};

static struct ctl_table ipv4_net_table[] = {
	{
		.procname	= "icmp_echo_ignore_all",
		.data		= &init_net.ipv4.sysctl_icmp_echo_ignore_all,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "icmp_echo_ignore_broadcasts",
		.data		= &init_net.ipv4.sysctl_icmp_echo_ignore_broadcasts,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "icmp_ignore_bogus_error_responses",
		.data		= &init_net.ipv4.sysctl_icmp_ignore_bogus_error_responses,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "icmp_errors_use_inbound_ifaddr",
		.data		= &init_net.ipv4.sysctl_icmp_errors_use_inbound_ifaddr,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "icmp_ratelimit",
		.data		= &init_net.ipv4.sysctl_icmp_ratelimit,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_ms_jiffies,
	},
	{
		.procname	= "icmp_ratemask",
		.data		= &init_net.ipv4.sysctl_icmp_ratemask,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "rt_cache_rebuild_count",
		.data		= &init_net.ipv4.sysctl_rt_cache_rebuild_count,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{ }
};

struct ctl_path net_ipv4_ctl_path[] = {
	{ .procname = "net", },
	{ .procname = "ipv4", },
	{ },
};
EXPORT_SYMBOL_GPL(net_ipv4_ctl_path);

static __net_init int ipv4_sysctl_init_net(struct net *net)
{
	struct ctl_table *table;

	table = ipv4_net_table;
	if (!net_eq(net, &init_net)) {
		table = kmemdup(table, sizeof(ipv4_net_table), GFP_KERNEL);
		if (table == NULL)
			goto err_alloc;

		table[0].data =
			&net->ipv4.sysctl_icmp_echo_ignore_all;
		table[1].data =
			&net->ipv4.sysctl_icmp_echo_ignore_broadcasts;
		table[2].data =
			&net->ipv4.sysctl_icmp_ignore_bogus_error_responses;
		table[3].data =
			&net->ipv4.sysctl_icmp_errors_use_inbound_ifaddr;
		table[4].data =
			&net->ipv4.sysctl_icmp_ratelimit;
		table[5].data =
			&net->ipv4.sysctl_icmp_ratemask;
		table[6].data =
			&net->ipv4.sysctl_rt_cache_rebuild_count;
	}

	net->ipv4.sysctl_rt_cache_rebuild_count = 4;

	net->ipv4.ipv4_hdr = register_net_sysctl_table(net,
			net_ipv4_ctl_path, table);
	if (net->ipv4.ipv4_hdr == NULL)
		goto err_reg;

	return 0;

err_reg:
	if (!net_eq(net, &init_net))
		kfree(table);
err_alloc:
	return -ENOMEM;
}

static __net_exit void ipv4_sysctl_exit_net(struct net *net)
{
	struct ctl_table *table;

	table = net->ipv4.ipv4_hdr->ctl_table_arg;
	unregister_net_sysctl_table(net->ipv4.ipv4_hdr);
	kfree(table);
}

static __net_initdata struct pernet_operations ipv4_sysctl_ops = {
	.init = ipv4_sysctl_init_net,
	.exit = ipv4_sysctl_exit_net,
};

static __init int sysctl_ipv4_init(void)
{
	struct ctl_table_header *hdr;
	struct ctl_table *i;

	for (i = ipv4_table; i->procname; i++) {
		if (strcmp(i->procname, "ip_local_reserved_ports") == 0) {
			i->data = sysctl_local_reserved_ports;
			break;
		}
	}
	if (!i->procname)
		return -EINVAL;

	hdr = register_sysctl_paths(net_ipv4_ctl_path, ipv4_table);
	if (hdr == NULL)
		return -ENOMEM;

	if (register_pernet_subsys(&ipv4_sysctl_ops)) {
		unregister_sysctl_table(hdr);
		return -ENOMEM;
	}

	return 0;
}

__initcall(sysctl_ipv4_init);