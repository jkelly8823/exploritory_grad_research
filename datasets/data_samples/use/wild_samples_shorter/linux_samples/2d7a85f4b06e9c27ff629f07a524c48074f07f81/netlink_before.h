}

enum netlink_skb_flags {
	NETLINK_SKB_MMAPED	= 0x1,		/* Packet data is mmaped */
	NETLINK_SKB_TX		= 0x2,		/* Packet was sent by userspace */
	NETLINK_SKB_DELIVERED	= 0x4,		/* Packet was delivered */
};

struct netlink_skb_parms {
	struct scm_creds	creds;		/* Skb credentials	*/