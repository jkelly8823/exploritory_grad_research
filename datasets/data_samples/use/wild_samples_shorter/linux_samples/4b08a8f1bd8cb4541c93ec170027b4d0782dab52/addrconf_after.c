	if (ifp->flags & IFA_F_OPTIMISTIC)
		addr_flags |= IFA_F_OPTIMISTIC;

	ift = ipv6_add_addr(idev, &addr, NULL, tmp_plen,
			    ipv6_addr_scope(&addr), addr_flags,
			    tmp_valid_lft, tmp_prefered_lft);
	if (IS_ERR(ift)) {
		in6_ifa_put(ifp);
		in6_dev_put(idev);
		pr_info("%s: retry temporary address regeneration\n", __func__);
		tmpaddr = &addr;