uint16_t __cachemode2pte_tbl[_PAGE_CACHE_MODE_NUM] = {
	[_PAGE_CACHE_MODE_WB]		= 0,
	[_PAGE_CACHE_MODE_WC]		= _PAGE_PWT,
	[_PAGE_CACHE_MODE_UC_MINUS]	= _PAGE_PCD,
	[_PAGE_CACHE_MODE_UC]		= _PAGE_PCD | _PAGE_PWT,
	[_PAGE_CACHE_MODE_WT]		= _PAGE_PCD,
	[_PAGE_CACHE_MODE_WP]		= _PAGE_PCD,
};
EXPORT_SYMBOL(__cachemode2pte_tbl);
uint8_t __pte2cachemode_tbl[8] = {
	[__pte2cm_idx(0)] = _PAGE_CACHE_MODE_WB,
	[__pte2cm_idx(_PAGE_PWT)] = _PAGE_CACHE_MODE_WC,
	[__pte2cm_idx(_PAGE_PCD)] = _PAGE_CACHE_MODE_UC_MINUS,
	[__pte2cm_idx(_PAGE_PWT | _PAGE_PCD)] = _PAGE_CACHE_MODE_UC,
	[__pte2cm_idx(_PAGE_PAT)] = _PAGE_CACHE_MODE_WB,
	[__pte2cm_idx(_PAGE_PWT | _PAGE_PAT)] = _PAGE_CACHE_MODE_WC,
	[__pte2cm_idx(_PAGE_PCD | _PAGE_PAT)] = _PAGE_CACHE_MODE_UC_MINUS,
	[__pte2cm_idx(_PAGE_PWT | _PAGE_PCD | _PAGE_PAT)] = _PAGE_CACHE_MODE_UC,
};
EXPORT_SYMBOL(__pte2cachemode_tbl);

static unsigned long __initdata pgt_buf_start;
static unsigned long __initdata pgt_buf_end;
static unsigned long __initdata pgt_buf_top;

static unsigned long min_pfn_mapped;

static bool __initdata can_use_brk_pgt = true;

/*
 * Pages returned are already directly mapped.
 *
 * Changing that is likely to break Xen, see commit:
 *
 *    279b706 x86,xen: introduce x86_init.mapping.pagetable_reserve
 *
 * for detailed information.
 */
__ref void *alloc_low_pages(unsigned int num)
{
	unsigned long pfn;
	int i;

	if (after_bootmem) {
		unsigned int order;

		order = get_order((unsigned long)num << PAGE_SHIFT);
		return (void *)__get_free_pages(GFP_ATOMIC | __GFP_NOTRACK |
						__GFP_ZERO, order);
	}

	if ((pgt_buf_end + num) > pgt_buf_top || !can_use_brk_pgt) {
		unsigned long ret;
		if (min_pfn_mapped >= max_pfn_mapped)
			panic("alloc_low_pages: ran out of memory");
		ret = memblock_find_in_range(min_pfn_mapped << PAGE_SHIFT,
					max_pfn_mapped << PAGE_SHIFT,
					PAGE_SIZE * num , PAGE_SIZE);
		if (!ret)
			panic("alloc_low_pages: can not alloc memory");
		memblock_reserve(ret, PAGE_SIZE * num);
		pfn = ret >> PAGE_SHIFT;
	} else {
		pfn = pgt_buf_end;
		pgt_buf_end += num;
		printk(KERN_DEBUG "BRK [%#010lx, %#010lx] PGTABLE\n",
			pfn << PAGE_SHIFT, (pgt_buf_end << PAGE_SHIFT) - 1);
	}

	for (i = 0; i < num; i++) {
		void *adr;

		adr = __va((pfn + i) << PAGE_SHIFT);
		clear_page(adr);
	}

	return __va(pfn << PAGE_SHIFT);
}

/* need 3 4k for initial PMD_SIZE,  3 4k for 0-ISA_END_ADDRESS */
#define INIT_PGT_BUF_SIZE	(6 * PAGE_SIZE)
RESERVE_BRK(early_pgt_alloc, INIT_PGT_BUF_SIZE);
void  __init early_alloc_pgt_buf(void)
{
	unsigned long tables = INIT_PGT_BUF_SIZE;
	phys_addr_t base;

	base = __pa(extend_brk(tables, PAGE_SIZE));

	pgt_buf_start = base >> PAGE_SHIFT;
	pgt_buf_end = pgt_buf_start;
	pgt_buf_top = pgt_buf_start + (tables >> PAGE_SHIFT);
}

int after_bootmem;

int direct_gbpages
#ifdef CONFIG_DIRECT_GBPAGES
				= 1
#endif
;

static void __init init_gbpages(void)
{
#ifdef CONFIG_X86_64
	if (direct_gbpages && cpu_has_gbpages)
		printk(KERN_INFO "Using GB pages for direct mapping\n");
	else
		direct_gbpages = 0;
#endif
}

struct map_range {
	unsigned long start;
	unsigned long end;
	unsigned page_size_mask;
};

static int page_size_mask;

static void __init probe_page_size_mask(void)
{
	init_gbpages();

#if !defined(CONFIG_DEBUG_PAGEALLOC) && !defined(CONFIG_KMEMCHECK)
	/*
	 * For CONFIG_DEBUG_PAGEALLOC, identity mapping will use small pages.
	 * This will simplify cpa(), which otherwise needs to support splitting
	 * large pages into small in interrupt context, etc.
	 */
	if (direct_gbpages)
		page_size_mask |= 1 << PG_LEVEL_1G;
	if (cpu_has_pse)
		page_size_mask |= 1 << PG_LEVEL_2M;
#endif

	/* Enable PSE if available */
	if (cpu_has_pse)
		set_in_cr4(X86_CR4_PSE);

	/* Enable PGE if available */
	if (cpu_has_pge) {
		set_in_cr4(X86_CR4_PGE);
		__supported_pte_mask |= _PAGE_GLOBAL;
	}
}

#ifdef CONFIG_X86_32
#define NR_RANGE_MR 3
#else /* CONFIG_X86_64 */
#define NR_RANGE_MR 5
#endif

static int __meminit save_mr(struct map_range *mr, int nr_range,
			     unsigned long start_pfn, unsigned long end_pfn,
			     unsigned long page_size_mask)
{
	if (start_pfn < end_pfn) {
		if (nr_range >= NR_RANGE_MR)
			panic("run out of range for init_memory_mapping\n");
		mr[nr_range].start = start_pfn<<PAGE_SHIFT;
		mr[nr_range].end   = end_pfn<<PAGE_SHIFT;
		mr[nr_range].page_size_mask = page_size_mask;
		nr_range++;
	}

	return nr_range;
}

/*
 * adjust the page_size_mask for small range to go with
 *	big page size instead small one if nearby are ram too.
 */
static void __init_refok adjust_range_page_size_mask(struct map_range *mr,
							 int nr_range)
{
	int i;

	for (i = 0; i < nr_range; i++) {
		if ((page_size_mask & (1<<PG_LEVEL_2M)) &&
		    !(mr[i].page_size_mask & (1<<PG_LEVEL_2M))) {
			unsigned long start = round_down(mr[i].start, PMD_SIZE);
			unsigned long end = round_up(mr[i].end, PMD_SIZE);

#ifdef CONFIG_X86_32
			if ((end >> PAGE_SHIFT) > max_low_pfn)
				continue;
#endif

			if (memblock_is_region_memory(start, end - start))
				mr[i].page_size_mask |= 1<<PG_LEVEL_2M;
		}
		if ((page_size_mask & (1<<PG_LEVEL_1G)) &&
		    !(mr[i].page_size_mask & (1<<PG_LEVEL_1G))) {
			unsigned long start = round_down(mr[i].start, PUD_SIZE);
			unsigned long end = round_up(mr[i].end, PUD_SIZE);

			if (memblock_is_region_memory(start, end - start))
				mr[i].page_size_mask |= 1<<PG_LEVEL_1G;
		}
	}
}

static int __meminit split_mem_range(struct map_range *mr, int nr_range,
				     unsigned long start,
				     unsigned long end)
{
	unsigned long start_pfn, end_pfn, limit_pfn;
	unsigned long pfn;
	int i;

	limit_pfn = PFN_DOWN(end);

	/* head if not big page alignment ? */
	pfn = start_pfn = PFN_DOWN(start);
#ifdef CONFIG_X86_32
	/*
	 * Don't use a large page for the first 2/4MB of memory
	 * because there are often fixed size MTRRs in there
	 * and overlapping MTRRs into large pages can cause
	 * slowdowns.
	 */
	if (pfn == 0)
		end_pfn = PFN_DOWN(PMD_SIZE);
	else
		end_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
#else /* CONFIG_X86_64 */
	end_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
#endif
	if (end_pfn > limit_pfn)
		end_pfn = limit_pfn;
	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn, 0);
		pfn = end_pfn;
	}

	/* big page (2M) range */
	start_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
#ifdef CONFIG_X86_32
	end_pfn = round_down(limit_pfn, PFN_DOWN(PMD_SIZE));
#else /* CONFIG_X86_64 */
	end_pfn = round_up(pfn, PFN_DOWN(PUD_SIZE));
	if (end_pfn > round_down(limit_pfn, PFN_DOWN(PMD_SIZE)))
		end_pfn = round_down(limit_pfn, PFN_DOWN(PMD_SIZE));
#endif

	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn,
				page_size_mask & (1<<PG_LEVEL_2M));
		pfn = end_pfn;
	}

#ifdef CONFIG_X86_64
	/* big page (1G) range */
	start_pfn = round_up(pfn, PFN_DOWN(PUD_SIZE));
	end_pfn = round_down(limit_pfn, PFN_DOWN(PUD_SIZE));
	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn,
				page_size_mask &
				 ((1<<PG_LEVEL_2M)|(1<<PG_LEVEL_1G)));
		pfn = end_pfn;
	}

	/* tail is not big page (1G) alignment */
	start_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
	end_pfn = round_down(limit_pfn, PFN_DOWN(PMD_SIZE));
	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn,
				page_size_mask & (1<<PG_LEVEL_2M));
		pfn = end_pfn;
	}
#endif

	/* tail is not big page (2M) alignment */
	start_pfn = pfn;
	end_pfn = limit_pfn;
	nr_range = save_mr(mr, nr_range, start_pfn, end_pfn, 0);

	if (!after_bootmem)
		adjust_range_page_size_mask(mr, nr_range);

	/* try to merge same page size and continuous */
	for (i = 0; nr_range > 1 && i < nr_range - 1; i++) {
		unsigned long old_start;
		if (mr[i].end != mr[i+1].start ||
		    mr[i].page_size_mask != mr[i+1].page_size_mask)
			continue;
		/* move it */
		old_start = mr[i].start;
		memmove(&mr[i], &mr[i+1],
			(nr_range - 1 - i) * sizeof(struct map_range));
		mr[i--].start = old_start;
		nr_range--;
	}

	for (i = 0; i < nr_range; i++)
		printk(KERN_DEBUG " [mem %#010lx-%#010lx] page %s\n",
				mr[i].start, mr[i].end - 1,
			(mr[i].page_size_mask & (1<<PG_LEVEL_1G))?"1G":(
			 (mr[i].page_size_mask & (1<<PG_LEVEL_2M))?"2M":"4k"));

	return nr_range;
}

struct range pfn_mapped[E820_X_MAX];
int nr_pfn_mapped;

static void add_pfn_range_mapped(unsigned long start_pfn, unsigned long end_pfn)
{
	nr_pfn_mapped = add_range_with_merge(pfn_mapped, E820_X_MAX,
					     nr_pfn_mapped, start_pfn, end_pfn);
	nr_pfn_mapped = clean_sort_range(pfn_mapped, E820_X_MAX);

	max_pfn_mapped = max(max_pfn_mapped, end_pfn);

	if (start_pfn < (1UL<<(32-PAGE_SHIFT)))
		max_low_pfn_mapped = max(max_low_pfn_mapped,
					 min(end_pfn, 1UL<<(32-PAGE_SHIFT)));
}

bool pfn_range_is_mapped(unsigned long start_pfn, unsigned long end_pfn)
{
	int i;

	for (i = 0; i < nr_pfn_mapped; i++)
		if ((start_pfn >= pfn_mapped[i].start) &&
		    (end_pfn <= pfn_mapped[i].end))
			return true;

	return false;
}

/*
 * Setup the direct mapping of the physical memory at PAGE_OFFSET.
 * This runs before bootmem is initialized and gets pages directly from
 * the physical memory. To access them they are temporarily mapped.
 */
unsigned long __init_refok init_memory_mapping(unsigned long start,
					       unsigned long end)
{
	struct map_range mr[NR_RANGE_MR];
	unsigned long ret = 0;
	int nr_range, i;

	pr_info("init_memory_mapping: [mem %#010lx-%#010lx]\n",
	       start, end - 1);

	memset(mr, 0, sizeof(mr));
	nr_range = split_mem_range(mr, 0, start, end);

	for (i = 0; i < nr_range; i++)
		ret = kernel_physical_mapping_init(mr[i].start, mr[i].end,
						   mr[i].page_size_mask);

	add_pfn_range_mapped(start >> PAGE_SHIFT, ret >> PAGE_SHIFT);

	return ret >> PAGE_SHIFT;
}

/*
 * We need to iterate through the E820 memory map and create direct mappings
 * for only E820_RAM and E820_KERN_RESERVED regions. We cannot simply
 * create direct mappings for all pfns from [0 to max_low_pfn) and
 * [4GB to max_pfn) because of possible memory holes in high addresses
 * that cannot be marked as UC by fixed/variable range MTRRs.
 * Depending on the alignment of E820 ranges, this may possibly result
 * in using smaller size (i.e. 4K instead of 2M or 1G) page tables.
 *
 * init_mem_mapping() calls init_range_memory_mapping() with big range.
 * That range would have hole in the middle or ends, and only ram parts
 * will be mapped in init_range_memory_mapping().
 */
static unsigned long __init init_range_memory_mapping(
					   unsigned long r_start,
					   unsigned long r_end)
{
	unsigned long start_pfn, end_pfn;
	unsigned long mapped_ram_size = 0;
	int i;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, NULL) {
		u64 start = clamp_val(PFN_PHYS(start_pfn), r_start, r_end);
		u64 end = clamp_val(PFN_PHYS(end_pfn), r_start, r_end);
		if (start >= end)
			continue;

		/*
		 * if it is overlapping with brk pgt, we need to
		 * alloc pgt buf from memblock instead.
		 */
		can_use_brk_pgt = max(start, (u64)pgt_buf_end<<PAGE_SHIFT) >=
				    min(end, (u64)pgt_buf_top<<PAGE_SHIFT);
		init_memory_mapping(start, end);
		mapped_ram_size += end - start;
		can_use_brk_pgt = true;
	}

	return mapped_ram_size;
}

static unsigned long __init get_new_step_size(unsigned long step_size)
{
	/*
	 * Initial mapped size is PMD_SIZE (2M).
	 * We can not set step_size to be PUD_SIZE (1G) yet.
	 * In worse case, when we cross the 1G boundary, and
	 * PG_LEVEL_2M is not set, we will need 1+1+512 pages (2M + 8k)
	 * to map 1G range with PTE. Hence we use one less than the
	 * difference of page table level shifts.
	 *
	 * Don't need to worry about overflow in the top-down case, on 32bit,
	 * when step_size is 0, round_down() returns 0 for start, and that
	 * turns it into 0x100000000ULL.
	 * In the bottom-up case, round_up(x, 0) returns 0 though too, which
	 * needs to be taken into consideration by the code below.
	 */
	return step_size << (PMD_SHIFT - PAGE_SHIFT - 1);
}

/**
 * memory_map_top_down - Map [map_start, map_end) top down
 * @map_start: start address of the target memory range
 * @map_end: end address of the target memory range
 *
 * This function will setup direct mapping for memory range
 * [map_start, map_end) in top-down. That said, the page tables
 * will be allocated at the end of the memory, and we map the
 * memory in top-down.
 */
static void __init memory_map_top_down(unsigned long map_start,
				       unsigned long map_end)
{
	unsigned long real_end, start, last_start;
	unsigned long step_size;
	unsigned long addr;
	unsigned long mapped_ram_size = 0;

	/* xen has big range in reserved near end of ram, skip it at first.*/
	addr = memblock_find_in_range(map_start, map_end, PMD_SIZE, PMD_SIZE);
	real_end = addr + PMD_SIZE;

	/* step_size need to be small so pgt_buf from BRK could cover it */
	step_size = PMD_SIZE;
	max_pfn_mapped = 0; /* will get exact value next */
	min_pfn_mapped = real_end >> PAGE_SHIFT;
	last_start = start = real_end;

	/*
	 * We start from the top (end of memory) and go to the bottom.
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (last_start > map_start) {
		if (last_start > step_size) {
			start = round_down(last_start - 1, step_size);
			if (start < map_start)
				start = map_start;
		} else
			start = map_start;
		mapped_ram_size += init_range_memory_mapping(start,
							last_start);
		last_start = start;
		min_pfn_mapped = last_start >> PAGE_SHIFT;
		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}

	if (real_end < map_end)
		init_range_memory_mapping(real_end, map_end);
}

/**
 * memory_map_bottom_up - Map [map_start, map_end) bottom up
 * @map_start: start address of the target memory range
 * @map_end: end address of the target memory range
 *
 * This function will setup direct mapping for memory range
 * [map_start, map_end) in bottom-up. Since we have limited the
 * bottom-up allocation above the kernel, the page tables will
 * be allocated just above the kernel and we map the memory
 * in [map_start, map_end) in bottom-up.
 */
static void __init memory_map_bottom_up(unsigned long map_start,
					unsigned long map_end)
{
	unsigned long next, start;
	unsigned long mapped_ram_size = 0;
	/* step_size need to be small so pgt_buf from BRK could cover it */
	unsigned long step_size = PMD_SIZE;

	start = map_start;
	min_pfn_mapped = start >> PAGE_SHIFT;

	/*
	 * We start from the bottom (@map_start) and go to the top (@map_end).
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (start < map_end) {
		if (step_size && map_end - start > step_size) {
			next = round_up(start + 1, step_size);
			if (next > map_end)
				next = map_end;
		} else {
			next = map_end;
		}

		mapped_ram_size += init_range_memory_mapping(start, next);
		start = next;

		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}
}

void __init init_mem_mapping(void)
{
	unsigned long end;

	probe_page_size_mask();

#ifdef CONFIG_X86_64
	end = max_pfn << PAGE_SHIFT;
#else
	end = max_low_pfn << PAGE_SHIFT;
#endif

	/* the ISA range is always mapped regardless of memory holes */
	init_memory_mapping(0, ISA_END_ADDRESS);

	/*
	 * If the allocation is in bottom-up direction, we setup direct mapping
	 * in bottom-up, otherwise we setup direct mapping in top-down.
	 */
	if (memblock_bottom_up()) {
		unsigned long kernel_end = __pa_symbol(_end);

		/*
		 * we need two separate calls here. This is because we want to
		 * allocate page tables above the kernel. So we first map
		 * [kernel_end, end) to make memory above the kernel be mapped
		 * as soon as possible. And then use page tables allocated above
		 * the kernel to map [ISA_END_ADDRESS, kernel_end).
		 */
		memory_map_bottom_up(kernel_end, end);
		memory_map_bottom_up(ISA_END_ADDRESS, kernel_end);
	} else {
		memory_map_top_down(ISA_END_ADDRESS, end);
	}

#ifdef CONFIG_X86_64
	if (max_pfn > max_low_pfn) {
		/* can we preseve max_low_pfn ?*/
		max_low_pfn = max_pfn;
	}
#else
	early_ioremap_page_table_range_init();
#endif

	load_cr3(swapper_pg_dir);
	__flush_tlb_all();

	early_memtest(0, max_pfn_mapped << PAGE_SHIFT);
}

/*
 * devmem_is_allowed() checks to see if /dev/mem access to a certain address
 * is valid. The argument is a physical page number.
 *
 *
 * On x86, access has to be given to the first megabyte of ram because that area
 * contains bios code and data regions used by X and dosemu and similar apps.
 * Access has to be given to non-kernel-ram areas as well, these contain the PCI
 * mmio resources as well as potential bios/acpi data regions.
 */
int devmem_is_allowed(unsigned long pagenr)
{
	if (pagenr < 256)
		return 1;
	if (iomem_is_exclusive(pagenr << PAGE_SHIFT))
		return 0;
	if (!page_is_ram(pagenr))
		return 1;
	return 0;
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long begin_aligned, end_aligned;

	/* Make sure boundaries are page aligned */
	begin_aligned = PAGE_ALIGN(begin);
	end_aligned   = end & PAGE_MASK;

	if (WARN_ON(begin_aligned != begin || end_aligned != end)) {
		begin = begin_aligned;
		end   = end_aligned;
	}

	if (begin >= end)
		return;

	/*
	 * If debugging page accesses then do not free this memory but
	 * mark them not present - any buggy init-section access will
	 * create a kernel page fault:
	 */
#ifdef CONFIG_DEBUG_PAGEALLOC
	printk(KERN_INFO "debug: unmapping init [mem %#010lx-%#010lx]\n",
		begin, end - 1);
	set_memory_np(begin, (end - begin) >> PAGE_SHIFT);
#else
	/*
	 * We just marked the kernel text read only above, now that
	 * we are going to free part of that, we need to make that
	 * writeable and non-executable first.
	 */
	set_memory_nx(begin, (end - begin) >> PAGE_SHIFT);
	set_memory_rw(begin, (end - begin) >> PAGE_SHIFT);

	free_reserved_area((void *)begin, (void *)end, POISON_FREE_INITMEM, what);
#endif
}

void free_initmem(void)
{
	free_init_pages("unused kernel",
			(unsigned long)(&__init_begin),
			(unsigned long)(&__init_end));
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
#ifdef CONFIG_MICROCODE_EARLY
	/*
	 * Remember, initrd memory may contain microcode or other useful things.
	 * Before we lose initrd mem, we need to find a place to hold them
	 * now that normal virtual memory is enabled.
	 */
	save_microcode_in_initrd();
#endif

	/*
	 * end could be not aligned, and We can not align that,
	 * decompresser could be confused by aligned initrd_end
	 * We already reserve the end partial page before in
	 *   - i386_start_kernel()
	 *   - x86_64_start_kernel()
	 *   - relocate_initrd()
	 * So here We can do PAGE_ALIGN() safely to get partial page to be freed
	 */
	free_init_pages("initrd", start, PAGE_ALIGN(end));
}
#endif

void __init zone_sizes_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

#ifdef CONFIG_ZONE_DMA
	max_zone_pfns[ZONE_DMA]		= min(MAX_DMA_PFN, max_low_pfn);
#endif
#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32]	= min(MAX_DMA32_PFN, max_low_pfn);
#endif
	max_zone_pfns[ZONE_NORMAL]	= max_low_pfn;
#ifdef CONFIG_HIGHMEM
	max_zone_pfns[ZONE_HIGHMEM]	= max_pfn;
#endif

	free_area_init_nodes(max_zone_pfns);
}

void update_cache_mode_entry(unsigned entry, enum page_cache_mode cache)
{
	/* entry 0 MUST be WB (hardwired to speed up translations) */
	BUG_ON(!entry && cache != _PAGE_CACHE_MODE_WB);

	__cachemode2pte_tbl[cache] = __cm_idx2pte(entry);
	__pte2cachemode_tbl[entry] = cache;
}
uint8_t __pte2cachemode_tbl[8] = {
	[__pte2cm_idx(0)] = _PAGE_CACHE_MODE_WB,
	[__pte2cm_idx(_PAGE_PWT)] = _PAGE_CACHE_MODE_WC,
	[__pte2cm_idx(_PAGE_PCD)] = _PAGE_CACHE_MODE_UC_MINUS,
	[__pte2cm_idx(_PAGE_PWT | _PAGE_PCD)] = _PAGE_CACHE_MODE_UC,
	[__pte2cm_idx(_PAGE_PAT)] = _PAGE_CACHE_MODE_WB,
	[__pte2cm_idx(_PAGE_PWT | _PAGE_PAT)] = _PAGE_CACHE_MODE_WC,
	[__pte2cm_idx(_PAGE_PCD | _PAGE_PAT)] = _PAGE_CACHE_MODE_UC_MINUS,
	[__pte2cm_idx(_PAGE_PWT | _PAGE_PCD | _PAGE_PAT)] = _PAGE_CACHE_MODE_UC,
};
EXPORT_SYMBOL(__pte2cachemode_tbl);

static unsigned long __initdata pgt_buf_start;
static unsigned long __initdata pgt_buf_end;
static unsigned long __initdata pgt_buf_top;

static unsigned long min_pfn_mapped;

static bool __initdata can_use_brk_pgt = true;

/*
 * Pages returned are already directly mapped.
 *
 * Changing that is likely to break Xen, see commit:
 *
 *    279b706 x86,xen: introduce x86_init.mapping.pagetable_reserve
 *
 * for detailed information.
 */
__ref void *alloc_low_pages(unsigned int num)
{
	unsigned long pfn;
	int i;

	if (after_bootmem) {
		unsigned int order;

		order = get_order((unsigned long)num << PAGE_SHIFT);
		return (void *)__get_free_pages(GFP_ATOMIC | __GFP_NOTRACK |
						__GFP_ZERO, order);
	}

	if ((pgt_buf_end + num) > pgt_buf_top || !can_use_brk_pgt) {
		unsigned long ret;
		if (min_pfn_mapped >= max_pfn_mapped)
			panic("alloc_low_pages: ran out of memory");
		ret = memblock_find_in_range(min_pfn_mapped << PAGE_SHIFT,
					max_pfn_mapped << PAGE_SHIFT,
					PAGE_SIZE * num , PAGE_SIZE);
		if (!ret)
			panic("alloc_low_pages: can not alloc memory");
		memblock_reserve(ret, PAGE_SIZE * num);
		pfn = ret >> PAGE_SHIFT;
	} else {
		pfn = pgt_buf_end;
		pgt_buf_end += num;
		printk(KERN_DEBUG "BRK [%#010lx, %#010lx] PGTABLE\n",
			pfn << PAGE_SHIFT, (pgt_buf_end << PAGE_SHIFT) - 1);
	}

	for (i = 0; i < num; i++) {
		void *adr;

		adr = __va((pfn + i) << PAGE_SHIFT);
		clear_page(adr);
	}

	return __va(pfn << PAGE_SHIFT);
}

/* need 3 4k for initial PMD_SIZE,  3 4k for 0-ISA_END_ADDRESS */
#define INIT_PGT_BUF_SIZE	(6 * PAGE_SIZE)
RESERVE_BRK(early_pgt_alloc, INIT_PGT_BUF_SIZE);
void  __init early_alloc_pgt_buf(void)
{
	unsigned long tables = INIT_PGT_BUF_SIZE;
	phys_addr_t base;

	base = __pa(extend_brk(tables, PAGE_SIZE));

	pgt_buf_start = base >> PAGE_SHIFT;
	pgt_buf_end = pgt_buf_start;
	pgt_buf_top = pgt_buf_start + (tables >> PAGE_SHIFT);
}

int after_bootmem;

int direct_gbpages
#ifdef CONFIG_DIRECT_GBPAGES
				= 1
#endif
;

static void __init init_gbpages(void)
{
#ifdef CONFIG_X86_64
	if (direct_gbpages && cpu_has_gbpages)
		printk(KERN_INFO "Using GB pages for direct mapping\n");
	else
		direct_gbpages = 0;
#endif
}

struct map_range {
	unsigned long start;
	unsigned long end;
	unsigned page_size_mask;
};

static int page_size_mask;

static void __init probe_page_size_mask(void)
{
	init_gbpages();

#if !defined(CONFIG_DEBUG_PAGEALLOC) && !defined(CONFIG_KMEMCHECK)
	/*
	 * For CONFIG_DEBUG_PAGEALLOC, identity mapping will use small pages.
	 * This will simplify cpa(), which otherwise needs to support splitting
	 * large pages into small in interrupt context, etc.
	 */
	if (direct_gbpages)
		page_size_mask |= 1 << PG_LEVEL_1G;
	if (cpu_has_pse)
		page_size_mask |= 1 << PG_LEVEL_2M;
#endif

	/* Enable PSE if available */
	if (cpu_has_pse)
		set_in_cr4(X86_CR4_PSE);

	/* Enable PGE if available */
	if (cpu_has_pge) {
		set_in_cr4(X86_CR4_PGE);
		__supported_pte_mask |= _PAGE_GLOBAL;
	}
}

#ifdef CONFIG_X86_32
#define NR_RANGE_MR 3
#else /* CONFIG_X86_64 */
#define NR_RANGE_MR 5
#endif

static int __meminit save_mr(struct map_range *mr, int nr_range,
			     unsigned long start_pfn, unsigned long end_pfn,
			     unsigned long page_size_mask)
{
	if (start_pfn < end_pfn) {
		if (nr_range >= NR_RANGE_MR)
			panic("run out of range for init_memory_mapping\n");
		mr[nr_range].start = start_pfn<<PAGE_SHIFT;
		mr[nr_range].end   = end_pfn<<PAGE_SHIFT;
		mr[nr_range].page_size_mask = page_size_mask;
		nr_range++;
	}

	return nr_range;
}

/*
 * adjust the page_size_mask for small range to go with
 *	big page size instead small one if nearby are ram too.
 */
static void __init_refok adjust_range_page_size_mask(struct map_range *mr,
							 int nr_range)
{
	int i;

	for (i = 0; i < nr_range; i++) {
		if ((page_size_mask & (1<<PG_LEVEL_2M)) &&
		    !(mr[i].page_size_mask & (1<<PG_LEVEL_2M))) {
			unsigned long start = round_down(mr[i].start, PMD_SIZE);
			unsigned long end = round_up(mr[i].end, PMD_SIZE);

#ifdef CONFIG_X86_32
			if ((end >> PAGE_SHIFT) > max_low_pfn)
				continue;
#endif

			if (memblock_is_region_memory(start, end - start))
				mr[i].page_size_mask |= 1<<PG_LEVEL_2M;
		}
		if ((page_size_mask & (1<<PG_LEVEL_1G)) &&
		    !(mr[i].page_size_mask & (1<<PG_LEVEL_1G))) {
			unsigned long start = round_down(mr[i].start, PUD_SIZE);
			unsigned long end = round_up(mr[i].end, PUD_SIZE);

			if (memblock_is_region_memory(start, end - start))
				mr[i].page_size_mask |= 1<<PG_LEVEL_1G;
		}
	}
}

static int __meminit split_mem_range(struct map_range *mr, int nr_range,
				     unsigned long start,
				     unsigned long end)
{
	unsigned long start_pfn, end_pfn, limit_pfn;
	unsigned long pfn;
	int i;

	limit_pfn = PFN_DOWN(end);

	/* head if not big page alignment ? */
	pfn = start_pfn = PFN_DOWN(start);
#ifdef CONFIG_X86_32
	/*
	 * Don't use a large page for the first 2/4MB of memory
	 * because there are often fixed size MTRRs in there
	 * and overlapping MTRRs into large pages can cause
	 * slowdowns.
	 */
	if (pfn == 0)
		end_pfn = PFN_DOWN(PMD_SIZE);
	else
		end_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
#else /* CONFIG_X86_64 */
	end_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
#endif
	if (end_pfn > limit_pfn)
		end_pfn = limit_pfn;
	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn, 0);
		pfn = end_pfn;
	}

	/* big page (2M) range */
	start_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
#ifdef CONFIG_X86_32
	end_pfn = round_down(limit_pfn, PFN_DOWN(PMD_SIZE));
#else /* CONFIG_X86_64 */
	end_pfn = round_up(pfn, PFN_DOWN(PUD_SIZE));
	if (end_pfn > round_down(limit_pfn, PFN_DOWN(PMD_SIZE)))
		end_pfn = round_down(limit_pfn, PFN_DOWN(PMD_SIZE));
#endif

	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn,
				page_size_mask & (1<<PG_LEVEL_2M));
		pfn = end_pfn;
	}

#ifdef CONFIG_X86_64
	/* big page (1G) range */
	start_pfn = round_up(pfn, PFN_DOWN(PUD_SIZE));
	end_pfn = round_down(limit_pfn, PFN_DOWN(PUD_SIZE));
	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn,
				page_size_mask &
				 ((1<<PG_LEVEL_2M)|(1<<PG_LEVEL_1G)));
		pfn = end_pfn;
	}

	/* tail is not big page (1G) alignment */
	start_pfn = round_up(pfn, PFN_DOWN(PMD_SIZE));
	end_pfn = round_down(limit_pfn, PFN_DOWN(PMD_SIZE));
	if (start_pfn < end_pfn) {
		nr_range = save_mr(mr, nr_range, start_pfn, end_pfn,
				page_size_mask & (1<<PG_LEVEL_2M));
		pfn = end_pfn;
	}
#endif

	/* tail is not big page (2M) alignment */
	start_pfn = pfn;
	end_pfn = limit_pfn;
	nr_range = save_mr(mr, nr_range, start_pfn, end_pfn, 0);

	if (!after_bootmem)
		adjust_range_page_size_mask(mr, nr_range);

	/* try to merge same page size and continuous */
	for (i = 0; nr_range > 1 && i < nr_range - 1; i++) {
		unsigned long old_start;
		if (mr[i].end != mr[i+1].start ||
		    mr[i].page_size_mask != mr[i+1].page_size_mask)
			continue;
		/* move it */
		old_start = mr[i].start;
		memmove(&mr[i], &mr[i+1],
			(nr_range - 1 - i) * sizeof(struct map_range));
		mr[i--].start = old_start;
		nr_range--;
	}

	for (i = 0; i < nr_range; i++)
		printk(KERN_DEBUG " [mem %#010lx-%#010lx] page %s\n",
				mr[i].start, mr[i].end - 1,
			(mr[i].page_size_mask & (1<<PG_LEVEL_1G))?"1G":(
			 (mr[i].page_size_mask & (1<<PG_LEVEL_2M))?"2M":"4k"));

	return nr_range;
}

struct range pfn_mapped[E820_X_MAX];
int nr_pfn_mapped;

static void add_pfn_range_mapped(unsigned long start_pfn, unsigned long end_pfn)
{
	nr_pfn_mapped = add_range_with_merge(pfn_mapped, E820_X_MAX,
					     nr_pfn_mapped, start_pfn, end_pfn);
	nr_pfn_mapped = clean_sort_range(pfn_mapped, E820_X_MAX);

	max_pfn_mapped = max(max_pfn_mapped, end_pfn);

	if (start_pfn < (1UL<<(32-PAGE_SHIFT)))
		max_low_pfn_mapped = max(max_low_pfn_mapped,
					 min(end_pfn, 1UL<<(32-PAGE_SHIFT)));
}

bool pfn_range_is_mapped(unsigned long start_pfn, unsigned long end_pfn)
{
	int i;

	for (i = 0; i < nr_pfn_mapped; i++)
		if ((start_pfn >= pfn_mapped[i].start) &&
		    (end_pfn <= pfn_mapped[i].end))
			return true;

	return false;
}

/*
 * Setup the direct mapping of the physical memory at PAGE_OFFSET.
 * This runs before bootmem is initialized and gets pages directly from
 * the physical memory. To access them they are temporarily mapped.
 */
unsigned long __init_refok init_memory_mapping(unsigned long start,
					       unsigned long end)
{
	struct map_range mr[NR_RANGE_MR];
	unsigned long ret = 0;
	int nr_range, i;

	pr_info("init_memory_mapping: [mem %#010lx-%#010lx]\n",
	       start, end - 1);

	memset(mr, 0, sizeof(mr));
	nr_range = split_mem_range(mr, 0, start, end);

	for (i = 0; i < nr_range; i++)
		ret = kernel_physical_mapping_init(mr[i].start, mr[i].end,
						   mr[i].page_size_mask);

	add_pfn_range_mapped(start >> PAGE_SHIFT, ret >> PAGE_SHIFT);

	return ret >> PAGE_SHIFT;
}

/*
 * We need to iterate through the E820 memory map and create direct mappings
 * for only E820_RAM and E820_KERN_RESERVED regions. We cannot simply
 * create direct mappings for all pfns from [0 to max_low_pfn) and
 * [4GB to max_pfn) because of possible memory holes in high addresses
 * that cannot be marked as UC by fixed/variable range MTRRs.
 * Depending on the alignment of E820 ranges, this may possibly result
 * in using smaller size (i.e. 4K instead of 2M or 1G) page tables.
 *
 * init_mem_mapping() calls init_range_memory_mapping() with big range.
 * That range would have hole in the middle or ends, and only ram parts
 * will be mapped in init_range_memory_mapping().
 */
static unsigned long __init init_range_memory_mapping(
					   unsigned long r_start,
					   unsigned long r_end)
{
	unsigned long start_pfn, end_pfn;
	unsigned long mapped_ram_size = 0;
	int i;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, NULL) {
		u64 start = clamp_val(PFN_PHYS(start_pfn), r_start, r_end);
		u64 end = clamp_val(PFN_PHYS(end_pfn), r_start, r_end);
		if (start >= end)
			continue;

		/*
		 * if it is overlapping with brk pgt, we need to
		 * alloc pgt buf from memblock instead.
		 */
		can_use_brk_pgt = max(start, (u64)pgt_buf_end<<PAGE_SHIFT) >=
				    min(end, (u64)pgt_buf_top<<PAGE_SHIFT);
		init_memory_mapping(start, end);
		mapped_ram_size += end - start;
		can_use_brk_pgt = true;
	}

	return mapped_ram_size;
}

static unsigned long __init get_new_step_size(unsigned long step_size)
{
	/*
	 * Initial mapped size is PMD_SIZE (2M).
	 * We can not set step_size to be PUD_SIZE (1G) yet.
	 * In worse case, when we cross the 1G boundary, and
	 * PG_LEVEL_2M is not set, we will need 1+1+512 pages (2M + 8k)
	 * to map 1G range with PTE. Hence we use one less than the
	 * difference of page table level shifts.
	 *
	 * Don't need to worry about overflow in the top-down case, on 32bit,
	 * when step_size is 0, round_down() returns 0 for start, and that
	 * turns it into 0x100000000ULL.
	 * In the bottom-up case, round_up(x, 0) returns 0 though too, which
	 * needs to be taken into consideration by the code below.
	 */
	return step_size << (PMD_SHIFT - PAGE_SHIFT - 1);
}

/**
 * memory_map_top_down - Map [map_start, map_end) top down
 * @map_start: start address of the target memory range
 * @map_end: end address of the target memory range
 *
 * This function will setup direct mapping for memory range
 * [map_start, map_end) in top-down. That said, the page tables
 * will be allocated at the end of the memory, and we map the
 * memory in top-down.
 */
static void __init memory_map_top_down(unsigned long map_start,
				       unsigned long map_end)
{
	unsigned long real_end, start, last_start;
	unsigned long step_size;
	unsigned long addr;
	unsigned long mapped_ram_size = 0;

	/* xen has big range in reserved near end of ram, skip it at first.*/
	addr = memblock_find_in_range(map_start, map_end, PMD_SIZE, PMD_SIZE);
	real_end = addr + PMD_SIZE;

	/* step_size need to be small so pgt_buf from BRK could cover it */
	step_size = PMD_SIZE;
	max_pfn_mapped = 0; /* will get exact value next */
	min_pfn_mapped = real_end >> PAGE_SHIFT;
	last_start = start = real_end;

	/*
	 * We start from the top (end of memory) and go to the bottom.
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (last_start > map_start) {
		if (last_start > step_size) {
			start = round_down(last_start - 1, step_size);
			if (start < map_start)
				start = map_start;
		} else
			start = map_start;
		mapped_ram_size += init_range_memory_mapping(start,
							last_start);
		last_start = start;
		min_pfn_mapped = last_start >> PAGE_SHIFT;
		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}

	if (real_end < map_end)
		init_range_memory_mapping(real_end, map_end);
}

/**
 * memory_map_bottom_up - Map [map_start, map_end) bottom up
 * @map_start: start address of the target memory range
 * @map_end: end address of the target memory range
 *
 * This function will setup direct mapping for memory range
 * [map_start, map_end) in bottom-up. Since we have limited the
 * bottom-up allocation above the kernel, the page tables will
 * be allocated just above the kernel and we map the memory
 * in [map_start, map_end) in bottom-up.
 */
static void __init memory_map_bottom_up(unsigned long map_start,
					unsigned long map_end)
{
	unsigned long next, start;
	unsigned long mapped_ram_size = 0;
	/* step_size need to be small so pgt_buf from BRK could cover it */
	unsigned long step_size = PMD_SIZE;

	start = map_start;
	min_pfn_mapped = start >> PAGE_SHIFT;

	/*
	 * We start from the bottom (@map_start) and go to the top (@map_end).
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (start < map_end) {
		if (step_size && map_end - start > step_size) {
			next = round_up(start + 1, step_size);
			if (next > map_end)
				next = map_end;
		} else {
			next = map_end;
		}

		mapped_ram_size += init_range_memory_mapping(start, next);
		start = next;

		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}
}

void __init init_mem_mapping(void)
{
	unsigned long end;

	probe_page_size_mask();

#ifdef CONFIG_X86_64
	end = max_pfn << PAGE_SHIFT;
#else
	end = max_low_pfn << PAGE_SHIFT;
#endif

	/* the ISA range is always mapped regardless of memory holes */
	init_memory_mapping(0, ISA_END_ADDRESS);

	/*
	 * If the allocation is in bottom-up direction, we setup direct mapping
	 * in bottom-up, otherwise we setup direct mapping in top-down.
	 */
	if (memblock_bottom_up()) {
		unsigned long kernel_end = __pa_symbol(_end);

		/*
		 * we need two separate calls here. This is because we want to
		 * allocate page tables above the kernel. So we first map
		 * [kernel_end, end) to make memory above the kernel be mapped
		 * as soon as possible. And then use page tables allocated above
		 * the kernel to map [ISA_END_ADDRESS, kernel_end).
		 */
		memory_map_bottom_up(kernel_end, end);
		memory_map_bottom_up(ISA_END_ADDRESS, kernel_end);
	} else {
		memory_map_top_down(ISA_END_ADDRESS, end);
	}

#ifdef CONFIG_X86_64
	if (max_pfn > max_low_pfn) {
		/* can we preseve max_low_pfn ?*/
		max_low_pfn = max_pfn;
	}
#else
	early_ioremap_page_table_range_init();
#endif

	load_cr3(swapper_pg_dir);
	__flush_tlb_all();

	early_memtest(0, max_pfn_mapped << PAGE_SHIFT);
}

/*
 * devmem_is_allowed() checks to see if /dev/mem access to a certain address
 * is valid. The argument is a physical page number.
 *
 *
 * On x86, access has to be given to the first megabyte of ram because that area
 * contains bios code and data regions used by X and dosemu and similar apps.
 * Access has to be given to non-kernel-ram areas as well, these contain the PCI
 * mmio resources as well as potential bios/acpi data regions.
 */
int devmem_is_allowed(unsigned long pagenr)
{
	if (pagenr < 256)
		return 1;
	if (iomem_is_exclusive(pagenr << PAGE_SHIFT))
		return 0;
	if (!page_is_ram(pagenr))
		return 1;
	return 0;
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long begin_aligned, end_aligned;

	/* Make sure boundaries are page aligned */
	begin_aligned = PAGE_ALIGN(begin);
	end_aligned   = end & PAGE_MASK;

	if (WARN_ON(begin_aligned != begin || end_aligned != end)) {
		begin = begin_aligned;
		end   = end_aligned;
	}

	if (begin >= end)
		return;

	/*
	 * If debugging page accesses then do not free this memory but
	 * mark them not present - any buggy init-section access will
	 * create a kernel page fault:
	 */
#ifdef CONFIG_DEBUG_PAGEALLOC
	printk(KERN_INFO "debug: unmapping init [mem %#010lx-%#010lx]\n",
		begin, end - 1);
	set_memory_np(begin, (end - begin) >> PAGE_SHIFT);
#else
	/*
	 * We just marked the kernel text read only above, now that
	 * we are going to free part of that, we need to make that
	 * writeable and non-executable first.
	 */
	set_memory_nx(begin, (end - begin) >> PAGE_SHIFT);
	set_memory_rw(begin, (end - begin) >> PAGE_SHIFT);

	free_reserved_area((void *)begin, (void *)end, POISON_FREE_INITMEM, what);
#endif
}

void free_initmem(void)
{
	free_init_pages("unused kernel",
			(unsigned long)(&__init_begin),
			(unsigned long)(&__init_end));
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
#ifdef CONFIG_MICROCODE_EARLY
	/*
	 * Remember, initrd memory may contain microcode or other useful things.
	 * Before we lose initrd mem, we need to find a place to hold them
	 * now that normal virtual memory is enabled.
	 */
	save_microcode_in_initrd();
#endif

	/*
	 * end could be not aligned, and We can not align that,
	 * decompresser could be confused by aligned initrd_end
	 * We already reserve the end partial page before in
	 *   - i386_start_kernel()
	 *   - x86_64_start_kernel()
	 *   - relocate_initrd()
	 * So here We can do PAGE_ALIGN() safely to get partial page to be freed
	 */
	free_init_pages("initrd", start, PAGE_ALIGN(end));
}
#endif

void __init zone_sizes_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

#ifdef CONFIG_ZONE_DMA
	max_zone_pfns[ZONE_DMA]		= min(MAX_DMA_PFN, max_low_pfn);
#endif
#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32]	= min(MAX_DMA32_PFN, max_low_pfn);
#endif
	max_zone_pfns[ZONE_NORMAL]	= max_low_pfn;
#ifdef CONFIG_HIGHMEM
	max_zone_pfns[ZONE_HIGHMEM]	= max_pfn;
#endif

	free_area_init_nodes(max_zone_pfns);
}

void update_cache_mode_entry(unsigned entry, enum page_cache_mode cache)
{
	/* entry 0 MUST be WB (hardwired to speed up translations) */
	BUG_ON(!entry && cache != _PAGE_CACHE_MODE_WB);

	__cachemode2pte_tbl[cache] = __cm_idx2pte(entry);
	__pte2cachemode_tbl[entry] = cache;
}
{
	/*
	 * Initial mapped size is PMD_SIZE (2M).
	 * We can not set step_size to be PUD_SIZE (1G) yet.
	 * In worse case, when we cross the 1G boundary, and
	 * PG_LEVEL_2M is not set, we will need 1+1+512 pages (2M + 8k)
	 * to map 1G range with PTE. Hence we use one less than the
	 * difference of page table level shifts.
	 *
	 * Don't need to worry about overflow in the top-down case, on 32bit,
	 * when step_size is 0, round_down() returns 0 for start, and that
	 * turns it into 0x100000000ULL.
	 * In the bottom-up case, round_up(x, 0) returns 0 though too, which
	 * needs to be taken into consideration by the code below.
	 */
	return step_size << (PMD_SHIFT - PAGE_SHIFT - 1);
}
{
	unsigned long real_end, start, last_start;
	unsigned long step_size;
	unsigned long addr;
	unsigned long mapped_ram_size = 0;

	/* xen has big range in reserved near end of ram, skip it at first.*/
	addr = memblock_find_in_range(map_start, map_end, PMD_SIZE, PMD_SIZE);
	real_end = addr + PMD_SIZE;

	/* step_size need to be small so pgt_buf from BRK could cover it */
	step_size = PMD_SIZE;
	max_pfn_mapped = 0; /* will get exact value next */
	min_pfn_mapped = real_end >> PAGE_SHIFT;
	last_start = start = real_end;

	/*
	 * We start from the top (end of memory) and go to the bottom.
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (last_start > map_start) {
		if (last_start > step_size) {
			start = round_down(last_start - 1, step_size);
			if (start < map_start)
				start = map_start;
		} else
			start = map_start;
		mapped_ram_size += init_range_memory_mapping(start,
							last_start);
		last_start = start;
		min_pfn_mapped = last_start >> PAGE_SHIFT;
		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}

	if (real_end < map_end)
		init_range_memory_mapping(real_end, map_end);
}
		if (last_start > step_size) {
			start = round_down(last_start - 1, step_size);
			if (start < map_start)
				start = map_start;
		} else
			start = map_start;
		mapped_ram_size += init_range_memory_mapping(start,
							last_start);
		last_start = start;
		min_pfn_mapped = last_start >> PAGE_SHIFT;
		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}

	if (real_end < map_end)
		init_range_memory_mapping(real_end, map_end);
}

/**
 * memory_map_bottom_up - Map [map_start, map_end) bottom up
 * @map_start: start address of the target memory range
 * @map_end: end address of the target memory range
 *
 * This function will setup direct mapping for memory range
 * [map_start, map_end) in bottom-up. Since we have limited the
 * bottom-up allocation above the kernel, the page tables will
 * be allocated just above the kernel and we map the memory
 * in [map_start, map_end) in bottom-up.
 */
static void __init memory_map_bottom_up(unsigned long map_start,
					unsigned long map_end)
{
	unsigned long next, start;
	unsigned long mapped_ram_size = 0;
	/* step_size need to be small so pgt_buf from BRK could cover it */
	unsigned long step_size = PMD_SIZE;

	start = map_start;
	min_pfn_mapped = start >> PAGE_SHIFT;

	/*
	 * We start from the bottom (@map_start) and go to the top (@map_end).
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (start < map_end) {
		if (step_size && map_end - start > step_size) {
			next = round_up(start + 1, step_size);
			if (next > map_end)
				next = map_end;
		} else {
			next = map_end;
		}

		mapped_ram_size += init_range_memory_mapping(start, next);
		start = next;

		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}
		if (last_start > step_size) {
			start = round_down(last_start - 1, step_size);
			if (start < map_start)
				start = map_start;
		} else
			start = map_start;
		mapped_ram_size += init_range_memory_mapping(start,
							last_start);
		last_start = start;
		min_pfn_mapped = last_start >> PAGE_SHIFT;
		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}

	if (real_end < map_end)
		init_range_memory_mapping(real_end, map_end);
}

/**
 * memory_map_bottom_up - Map [map_start, map_end) bottom up
 * @map_start: start address of the target memory range
 * @map_end: end address of the target memory range
 *
 * This function will setup direct mapping for memory range
 * [map_start, map_end) in bottom-up. Since we have limited the
 * bottom-up allocation above the kernel, the page tables will
 * be allocated just above the kernel and we map the memory
 * in [map_start, map_end) in bottom-up.
 */
static void __init memory_map_bottom_up(unsigned long map_start,
					unsigned long map_end)
{
	unsigned long next, start;
	unsigned long mapped_ram_size = 0;
	/* step_size need to be small so pgt_buf from BRK could cover it */
	unsigned long step_size = PMD_SIZE;

	start = map_start;
	min_pfn_mapped = start >> PAGE_SHIFT;

	/*
	 * We start from the bottom (@map_start) and go to the top (@map_end).
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (start < map_end) {
		if (step_size && map_end - start > step_size) {
			next = round_up(start + 1, step_size);
			if (next > map_end)
				next = map_end;
		} else {
			next = map_end;
		}

		mapped_ram_size += init_range_memory_mapping(start, next);
		start = next;

		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}
}

void __init init_mem_mapping(void)
{
	unsigned long end;

	probe_page_size_mask();

#ifdef CONFIG_X86_64
	end = max_pfn << PAGE_SHIFT;
#else
	end = max_low_pfn << PAGE_SHIFT;
#endif

	/* the ISA range is always mapped regardless of memory holes */
	init_memory_mapping(0, ISA_END_ADDRESS);

	/*
	 * If the allocation is in bottom-up direction, we setup direct mapping
	 * in bottom-up, otherwise we setup direct mapping in top-down.
	 */
	if (memblock_bottom_up()) {
		unsigned long kernel_end = __pa_symbol(_end);

		/*
		 * we need two separate calls here. This is because we want to
		 * allocate page tables above the kernel. So we first map
		 * [kernel_end, end) to make memory above the kernel be mapped
		 * as soon as possible. And then use page tables allocated above
		 * the kernel to map [ISA_END_ADDRESS, kernel_end).
		 */
		memory_map_bottom_up(kernel_end, end);
		memory_map_bottom_up(ISA_END_ADDRESS, kernel_end);
	} else {
		memory_map_top_down(ISA_END_ADDRESS, end);
	}

#ifdef CONFIG_X86_64
	if (max_pfn > max_low_pfn) {
		/* can we preseve max_low_pfn ?*/
		max_low_pfn = max_pfn;
	}
#else
	early_ioremap_page_table_range_init();
#endif

	load_cr3(swapper_pg_dir);
	__flush_tlb_all();

	early_memtest(0, max_pfn_mapped << PAGE_SHIFT);
}

/*
 * devmem_is_allowed() checks to see if /dev/mem access to a certain address
 * is valid. The argument is a physical page number.
 *
 *
 * On x86, access has to be given to the first megabyte of ram because that area
 * contains bios code and data regions used by X and dosemu and similar apps.
 * Access has to be given to non-kernel-ram areas as well, these contain the PCI
 * mmio resources as well as potential bios/acpi data regions.
 */
int devmem_is_allowed(unsigned long pagenr)
{
	if (pagenr < 256)
		return 1;
	if (iomem_is_exclusive(pagenr << PAGE_SHIFT))
		return 0;
	if (!page_is_ram(pagenr))
		return 1;
	return 0;
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long begin_aligned, end_aligned;

	/* Make sure boundaries are page aligned */
	begin_aligned = PAGE_ALIGN(begin);
	end_aligned   = end & PAGE_MASK;

	if (WARN_ON(begin_aligned != begin || end_aligned != end)) {
		begin = begin_aligned;
		end   = end_aligned;
	}

	if (begin >= end)
		return;

	/*
	 * If debugging page accesses then do not free this memory but
	 * mark them not present - any buggy init-section access will
	 * create a kernel page fault:
	 */
#ifdef CONFIG_DEBUG_PAGEALLOC
	printk(KERN_INFO "debug: unmapping init [mem %#010lx-%#010lx]\n",
		begin, end - 1);
	set_memory_np(begin, (end - begin) >> PAGE_SHIFT);
#else
	/*
	 * We just marked the kernel text read only above, now that
	 * we are going to free part of that, we need to make that
	 * writeable and non-executable first.
	 */
	set_memory_nx(begin, (end - begin) >> PAGE_SHIFT);
	set_memory_rw(begin, (end - begin) >> PAGE_SHIFT);

	free_reserved_area((void *)begin, (void *)end, POISON_FREE_INITMEM, what);
#endif
}

void free_initmem(void)
{
	free_init_pages("unused kernel",
			(unsigned long)(&__init_begin),
			(unsigned long)(&__init_end));
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
#ifdef CONFIG_MICROCODE_EARLY
	/*
	 * Remember, initrd memory may contain microcode or other useful things.
	 * Before we lose initrd mem, we need to find a place to hold them
	 * now that normal virtual memory is enabled.
	 */
	save_microcode_in_initrd();
#endif

	/*
	 * end could be not aligned, and We can not align that,
	 * decompresser could be confused by aligned initrd_end
	 * We already reserve the end partial page before in
	 *   - i386_start_kernel()
	 *   - x86_64_start_kernel()
	 *   - relocate_initrd()
	 * So here We can do PAGE_ALIGN() safely to get partial page to be freed
	 */
	free_init_pages("initrd", start, PAGE_ALIGN(end));
}
#endif

void __init zone_sizes_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

#ifdef CONFIG_ZONE_DMA
	max_zone_pfns[ZONE_DMA]		= min(MAX_DMA_PFN, max_low_pfn);
#endif
#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32]	= min(MAX_DMA32_PFN, max_low_pfn);
#endif
	max_zone_pfns[ZONE_NORMAL]	= max_low_pfn;
#ifdef CONFIG_HIGHMEM
	max_zone_pfns[ZONE_HIGHMEM]	= max_pfn;
#endif

	free_area_init_nodes(max_zone_pfns);
}

void update_cache_mode_entry(unsigned entry, enum page_cache_mode cache)
{
	/* entry 0 MUST be WB (hardwired to speed up translations) */
	BUG_ON(!entry && cache != _PAGE_CACHE_MODE_WB);

	__cachemode2pte_tbl[cache] = __cm_idx2pte(entry);
	__pte2cachemode_tbl[entry] = cache;
}
{
	unsigned long next, start;
	unsigned long mapped_ram_size = 0;
	/* step_size need to be small so pgt_buf from BRK could cover it */
	unsigned long step_size = PMD_SIZE;

	start = map_start;
	min_pfn_mapped = start >> PAGE_SHIFT;

	/*
	 * We start from the bottom (@map_start) and go to the top (@map_end).
	 * The memblock_find_in_range() gets us a block of RAM from the
	 * end of RAM in [min_pfn_mapped, max_pfn_mapped) used as new pages
	 * for page table.
	 */
	while (start < map_end) {
		if (step_size && map_end - start > step_size) {
			next = round_up(start + 1, step_size);
			if (next > map_end)
				next = map_end;
		} else {
			next = map_end;
		}

		mapped_ram_size += init_range_memory_mapping(start, next);
		start = next;

		if (mapped_ram_size >= step_size)
			step_size = get_new_step_size(step_size);
	}
}