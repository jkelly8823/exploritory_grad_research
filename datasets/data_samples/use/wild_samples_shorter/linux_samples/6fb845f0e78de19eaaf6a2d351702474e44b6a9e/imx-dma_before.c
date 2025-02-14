{
	struct imxdma_channel *imxdmac = (void *)data;
	struct imxdma_engine *imxdma = imxdmac->imxdma;
	struct imxdma_desc *desc;
	unsigned long flags;

	spin_lock_irqsave(&imxdma->lock, flags);

	list_move_tail(imxdmac->ld_active.next, &imxdmac->ld_free);

	if (!list_empty(&imxdmac->ld_queue)) {
		desc = list_first_entry(&imxdmac->ld_queue, struct imxdma_desc,
					node);
		list_move_tail(imxdmac->ld_queue.next, &imxdmac->ld_active);
		if (imxdma_xfer_desc(desc) < 0)
			dev_warn(imxdma->dev, "%s: channel: %d couldn't xfer desc\n",
				 __func__, imxdmac->channel);
	}
out: