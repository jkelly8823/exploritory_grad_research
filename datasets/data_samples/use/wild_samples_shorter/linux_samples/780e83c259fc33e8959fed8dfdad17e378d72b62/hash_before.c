u32 xenvif_set_hash_mapping(struct xenvif *vif, u32 gref, u32 len,
			    u32 off)
{
	u32 *mapping = &vif->hash.mapping[off];
	struct gnttab_copy copy_op = {
		.source.u.ref = gref,
		.source.domid = vif->domid,
		.dest.u.gmfn = virt_to_gfn(mapping),
		.dest.domid = DOMID_SELF,
		.dest.offset = xen_offset_in_page(mapping),
		.len = len * sizeof(u32),
		.flags = GNTCOPY_source_gref
	};

	if ((off + len > vif->hash.size) || copy_op.len > XEN_PAGE_SIZE)
		return XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER;

	while (len-- != 0)
		if (mapping[off++] >= vif->num_queues)
			return XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER;
