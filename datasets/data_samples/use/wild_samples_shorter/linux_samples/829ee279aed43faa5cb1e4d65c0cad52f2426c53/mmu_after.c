static inline bool is_last_gpte(struct kvm_mmu *mmu,
				unsigned level, unsigned gpte)
{
	/*
	 * The RHS has bit 7 set iff level < mmu->last_nonleaf_level.
	 * If it is clear, there are no large pages at this level, so clear
	 * PT_PAGE_SIZE_MASK in gpte if that is the case.
	 */
	gpte &= level - mmu->last_nonleaf_level;

	/*
	 * PT_PAGE_TABLE_LEVEL always terminates.  The RHS has bit 7 set
	 * iff level <= PT_PAGE_TABLE_LEVEL, which for our purpose means
	 * level == PT_PAGE_TABLE_LEVEL; set PT_PAGE_SIZE_MASK in gpte then.
	 */
	gpte |= level - PT_PAGE_TABLE_LEVEL - 1;

	return gpte & PT_PAGE_SIZE_MASK;
}

#define PTTYPE_EPT 18 /* arbitrary */