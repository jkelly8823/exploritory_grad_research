				ref = priv->rings[i].intf->ref[j];
				gnttab_end_foreign_access(ref, 0, 0);
			}
			free_pages_exact(priv->rings[i].data.in,
				   1UL << (priv->rings[i].intf->ring_order +
					   XEN_PAGE_SHIFT));
		}
		gnttab_end_foreign_access(priv->rings[i].ref, 0, 0);
		free_page((unsigned long)priv->rings[i].intf);
	}
	if (ret < 0)
		goto out;
	ring->ref = ret;
	bytes = alloc_pages_exact(1UL << (order + XEN_PAGE_SHIFT),
				  GFP_KERNEL | __GFP_ZERO);
	if (!bytes) {
		ret = -ENOMEM;
		goto out;
	}
	if (bytes) {
		for (i--; i >= 0; i--)
			gnttab_end_foreign_access(ring->intf->ref[i], 0, 0);
		free_pages_exact(bytes, 1UL << (order + XEN_PAGE_SHIFT));
	}
	gnttab_end_foreign_access(ring->ref, 0, 0);
	free_page((unsigned long)ring->intf);
	return ret;