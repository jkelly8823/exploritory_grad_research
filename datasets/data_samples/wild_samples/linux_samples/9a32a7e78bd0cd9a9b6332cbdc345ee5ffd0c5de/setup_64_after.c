{
#ifdef CONFIG_HARDLOCKUP_DETECTOR_PERF
	hardlockup_detector_disable();
#else
	if (firmware_has_feature(FW_FEATURE_LPAR))
		hardlockup_detector_disable();
#endif

	return 0;
}
early_initcall(disable_hardlockup_detector);

#ifdef CONFIG_PPC_BOOK3S_64
static enum l1d_flush_type enabled_flush_types;
static void *l1d_flush_fallback_area;
static bool no_rfi_flush;
static bool no_entry_flush;
static bool no_uaccess_flush;
bool rfi_flush;
bool entry_flush;
bool uaccess_flush;
DEFINE_STATIC_KEY_FALSE(uaccess_flush_key);
EXPORT_SYMBOL(uaccess_flush_key);

static int __init handle_no_rfi_flush(char *p)
{
	pr_info("rfi-flush: disabled on command line.");
	no_rfi_flush = true;
	return 0;
}
{
	pr_info("entry-flush: disabled on command line.");
	no_entry_flush = true;
	return 0;
}
early_param("no_entry_flush", handle_no_entry_flush);

static int __init handle_no_uaccess_flush(char *p)
{
	pr_info("uaccess-flush: disabled on command line.");
	no_uaccess_flush = true;
	return 0;
}
	} else {
		do_entry_flush_fixups(L1D_FLUSH_NONE);
	}

	entry_flush = enable;
}

void uaccess_flush_enable(bool enable)
{
	if (enable) {
		do_uaccess_flush_fixups(enabled_flush_types);
		static_branch_enable(&uaccess_flush_key);
		on_each_cpu(do_nothing, NULL, 1);
	} else {
		static_branch_disable(&uaccess_flush_key);
		do_uaccess_flush_fixups(L1D_FLUSH_NONE);
	}

	uaccess_flush = enable;
}
{
	if (cpu_mitigations_off())
		return;

	if (!no_entry_flush)
		entry_flush_enable(enable);
}

void setup_uaccess_flush(bool enable)
{
	if (cpu_mitigations_off())
		return;

	if (!no_uaccess_flush)
		uaccess_flush_enable(enable);
}

#ifdef CONFIG_DEBUG_FS
static int rfi_flush_set(void *data, u64 val)
{
	bool enable;

	if (val == 1)
		enable = true;
	else if (val == 0)
		enable = false;
	else
		return -EINVAL;

	/* Only do anything if we're changing state */
	if (enable != rfi_flush)
		rfi_flush_enable(enable);

	return 0;
}

static int rfi_flush_get(void *data, u64 *val)
{
	*val = rfi_flush ? 1 : 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_rfi_flush, rfi_flush_get, rfi_flush_set, "%llu\n");

static int entry_flush_set(void *data, u64 val)
{
	bool enable;

	if (val == 1)
		enable = true;
	else if (val == 0)
		enable = false;
	else
		return -EINVAL;

	/* Only do anything if we're changing state */
	if (enable != entry_flush)
		entry_flush_enable(enable);

	return 0;
}

static int entry_flush_get(void *data, u64 *val)
{
	*val = entry_flush ? 1 : 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_entry_flush, entry_flush_get, entry_flush_set, "%llu\n");

static int uaccess_flush_set(void *data, u64 val)
{
	bool enable;

	if (val == 1)
		enable = true;
	else if (val == 0)
		enable = false;
	else
		return -EINVAL;

	/* Only do anything if we're changing state */
	if (enable != uaccess_flush)
		uaccess_flush_enable(enable);

	return 0;
}

static int uaccess_flush_get(void *data, u64 *val)
{
	*val = uaccess_flush ? 1 : 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_uaccess_flush, uaccess_flush_get, uaccess_flush_set, "%llu\n");

static __init int rfi_flush_debugfs_init(void)
{
	debugfs_create_file("rfi_flush", 0600, powerpc_debugfs_root, NULL, &fops_rfi_flush);
	debugfs_create_file("entry_flush", 0600, powerpc_debugfs_root, NULL, &fops_entry_flush);
	debugfs_create_file("uaccess_flush", 0600, powerpc_debugfs_root, NULL, &fops_uaccess_flush);
	return 0;
}
device_initcall(rfi_flush_debugfs_init);
#endif
#endif /* CONFIG_PPC_BOOK3S_64 */
{
	*val = entry_flush ? 1 : 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_entry_flush, entry_flush_get, entry_flush_set, "%llu\n");

static int uaccess_flush_set(void *data, u64 val)
{
	bool enable;

	if (val == 1)
		enable = true;
	else if (val == 0)
		enable = false;
	else
		return -EINVAL;

	/* Only do anything if we're changing state */
	if (enable != uaccess_flush)
		uaccess_flush_enable(enable);

	return 0;
}