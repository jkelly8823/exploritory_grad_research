{
	dev->archdata.dma_ops = ops;
}

static inline void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				      struct iommu_ops *iommu, bool coherent)
{
	dev->archdata.dma_coherent = coherent;
	if (coherent)
		set_dma_ops(dev, &coherent_swiotlb_dma_ops);
}
#define arch_setup_dma_ops	arch_setup_dma_ops

/* do not use this function in a driver */
static inline bool is_device_dma_coherent(struct device *dev)
{
	return dev->archdata.dma_coherent;
}

#include <asm-generic/dma-mapping-common.h>

static inline dma_addr_t phys_to_dma(struct device *dev, phys_addr_t paddr)
{
	return (dma_addr_t)paddr;
}

static inline phys_addr_t dma_to_phys(struct device *dev, dma_addr_t dev_addr)
{
	return (phys_addr_t)dev_addr;
}

static inline int dma_mapping_error(struct device *dev, dma_addr_t dev_addr)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	debug_dma_mapping_error(dev, dev_addr);
	return ops->mapping_error(dev, dev_addr);
}

static inline int dma_supported(struct device *dev, u64 mask)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	return ops->dma_supported(dev, mask);
}

static inline int dma_set_mask(struct device *dev, u64 mask)
{
	if (!dev->dma_mask || !dma_supported(dev, mask))
		return -EIO;
	*dev->dma_mask = mask;

	return 0;
}

static inline bool dma_capable(struct device *dev, dma_addr_t addr, size_t size)
{
	if (!dev->dma_mask)
		return 0;

	return addr + size - 1 <= *dev->dma_mask;
}

static inline void dma_mark_clean(void *addr, size_t size)
{
}

#define dma_alloc_coherent(d, s, h, f)	dma_alloc_attrs(d, s, h, f, NULL)
#define dma_free_coherent(d, s, h, f)	dma_free_attrs(d, s, h, f, NULL)

static inline void *dma_alloc_attrs(struct device *dev, size_t size,
				    dma_addr_t *dma_handle, gfp_t flags,
				    struct dma_attrs *attrs)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	void *vaddr;

	if (dma_alloc_from_coherent(dev, size, dma_handle, &vaddr))
		return vaddr;

	vaddr = ops->alloc(dev, size, dma_handle, flags, attrs);
	debug_dma_alloc_coherent(dev, size, *dma_handle, vaddr);
	return vaddr;
}

static inline void dma_free_attrs(struct device *dev, size_t size,
				  void *vaddr, dma_addr_t dev_addr,
				  struct dma_attrs *attrs)
{
	struct dma_map_ops *ops = get_dma_ops(dev);

	if (dma_release_from_coherent(dev, get_order(size), vaddr))
		return;

	debug_dma_free_coherent(dev, size, vaddr, dev_addr);
	ops->free(dev, size, vaddr, dev_addr, attrs);
}

/*
 * There is no dma_cache_sync() implementation, so just return NULL here.
 */
static inline void *dma_alloc_noncoherent(struct device *dev, size_t size,
					  dma_addr_t *handle, gfp_t flags)
{
	return NULL;
}

static inline void dma_free_noncoherent(struct device *dev, size_t size,
					void *cpu_addr, dma_addr_t handle)
{
}

#endif	/* __KERNEL__ */
#endif	/* __ASM_DMA_MAPPING_H */