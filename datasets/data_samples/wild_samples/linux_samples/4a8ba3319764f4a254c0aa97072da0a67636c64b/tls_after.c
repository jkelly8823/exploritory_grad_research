{
	struct thread_struct *t = &current->thread;
	int idx;

	for (idx = 0; idx < GDT_ENTRY_TLS_ENTRIES; idx++)
		if (desc_empty(&t->tls_array[idx]))
			return idx + GDT_ENTRY_TLS_MIN;
	return -ESRCH;
}

static bool tls_desc_okay(const struct user_desc *info)
{
	/*
	 * For historical reasons (i.e. no one ever documented how any
	 * of the segmentation APIs work), user programs can and do
	 * assume that a struct user_desc that's all zeros except for
	 * entry_number means "no segment at all".  This never actually
	 * worked.  In fact, up to Linux 3.19, a struct user_desc like
	 * this would create a 16-bit read-write segment with base and
	 * limit both equal to zero.
	 *
	 * That was close enough to "no segment at all" until we
	 * hardened this function to disallow 16-bit TLS segments.  Fix
	 * it up by interpreting these zeroed segments the way that they
	 * were almost certainly intended to be interpreted.
	 *
	 * The correct way to ask for "no segment at all" is to specify
	 * a user_desc that satisfies LDT_empty.  To keep everything
	 * working, we accept both.
	 *
	 * Note that there's a similar kludge in modify_ldt -- look at
	 * the distinction between modes 1 and 0x11.
	 */
	if (LDT_empty(info) || LDT_zero(info))
		return true;

	/*
	 * espfix is required for 16-bit data segments, but espfix
	 * only works for LDT segments.
	 */
	if (!info->seg_32bit)
		return false;

	/* Only allow data segments in the TLS array. */
	if (info->contents > 1)
		return false;

	/*
	 * Non-present segments with DPL 3 present an interesting attack
	 * surface.  The kernel should handle such segments correctly,
	 * but TLS is very difficult to protect in a sandbox, so prevent
	 * such segments from being created.
	 *
	 * If userspace needs to remove a TLS entry, it can still delete
	 * it outright.
	 */
	if (info->seg_not_present)
		return false;

	return true;
}
{
	struct thread_struct *t = &p->thread;
	struct desc_struct *desc = &t->tls_array[idx - GDT_ENTRY_TLS_MIN];
	int cpu;

	/*
	 * We must not get preempted while modifying the TLS.
	 */
	cpu = get_cpu();

	while (n-- > 0) {
		if (LDT_empty(info) || LDT_zero(info))
			desc->a = desc->b = 0;
		else
			fill_ldt(desc, info);
		++info;
		++desc;
	}