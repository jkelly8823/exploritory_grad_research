MODULE_PARM_DESC(disable_hugepages,
		 "Disable VFIO IOMMU support for IOMMU hugepages.");

static unsigned int dma_entry_limit __read_mostly = U16_MAX;
module_param_named(dma_entry_limit, dma_entry_limit, uint, 0644);
MODULE_PARM_DESC(dma_entry_limit,
		 "Maximum number of user DMA mappings per container (65535).");

struct vfio_iommu {
	struct list_head	domain_list;
	struct vfio_domain	*external_domain; /* domain for external user */
	struct mutex		lock;
	struct rb_root		dma_list;
	struct blocking_notifier_head notifier;
	unsigned int		dma_avail;
	bool			v2;
	bool			nesting;
};

	vfio_unlink_dma(iommu, dma);
	put_task_struct(dma->task);
	kfree(dma);
	iommu->dma_avail++;
}

static unsigned long vfio_pgsize_bitmap(struct vfio_iommu *iommu)
{
		goto out_unlock;
	}

	if (!iommu->dma_avail) {
		ret = -ENOSPC;
		goto out_unlock;
	}

	dma = kzalloc(sizeof(*dma), GFP_KERNEL);
	if (!dma) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	iommu->dma_avail--;
	dma->iova = iova;
	dma->vaddr = vaddr;
	dma->prot = prot;


	INIT_LIST_HEAD(&iommu->domain_list);
	iommu->dma_list = RB_ROOT;
	iommu->dma_avail = dma_entry_limit;
	mutex_init(&iommu->lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&iommu->notifier);

	return iommu;