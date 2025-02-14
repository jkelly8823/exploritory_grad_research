{
	mm_segment_t old_fs;
	unsigned long asce, cr;

	old_fs = current->thread.mm_segment;
	if (old_fs & 1)
		return old_fs;
	current->thread.mm_segment |= 1;
	asce = S390_lowcore.kernel_asce;
	if (likely(old_fs == USER_DS)) {
		__ctl_store(cr, 1, 1);
		__ctl_load(asce, 7, 7);
		set_cpu_flag(CIF_ASCE_SECONDARY);
	}
	return old_fs;
}
EXPORT_SYMBOL(enable_sacf_uaccess);
