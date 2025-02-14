{
	unsigned i;

	for_each_possible_cpu(i)
		set_cpu_present(i, true);
}

void __init smp_init_cpus(void)
{
	unsigned i;
	unsigned int ncpus = get_core_count();
	unsigned int core_id = get_core_id();

	pr_info("%s: Core Count = %d\n", __func__, ncpus);
	pr_info("%s: Core Id = %d\n", __func__, core_id);

	if (ncpus > NR_CPUS) {
		ncpus = NR_CPUS;
		pr_info("%s: limiting core count by %d\n", __func__, ncpus);
	}
{
	unsigned i;
	unsigned int ncpus = get_core_count();
	unsigned int core_id = get_core_id();

	pr_info("%s: Core Count = %d\n", __func__, ncpus);
	pr_info("%s: Core Id = %d\n", __func__, core_id);

	if (ncpus > NR_CPUS) {
		ncpus = NR_CPUS;
		pr_info("%s: limiting core count by %d\n", __func__, ncpus);
	}
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	unsigned long ccount;
	int i;

#ifdef CONFIG_HOTPLUG_CPU
	WRITE_ONCE(cpu_start_id, cpu);
	/* Pairs with the third memw in the cpu_restart */
	mb();
	system_flush_invalidate_dcache_range((unsigned long)&cpu_start_id,
					     sizeof(cpu_start_id));
#endif
	smp_call_function_single(0, mx_cpu_start, (void *)cpu, 1);

	for (i = 0; i < 2; ++i) {
		do
			ccount = get_ccount();
		while (!ccount);

		WRITE_ONCE(cpu_start_ccount, ccount);

		do {
			/*
			 * Pairs with the first two memws in the
			 * .Lboot_secondary.
			 */
			mb();
			ccount = READ_ONCE(cpu_start_ccount);
		} while (ccount && time_before(jiffies, timeout));

		if (ccount) {
			smp_call_function_single(0, mx_cpu_stop,
						 (void *)cpu, 1);
			WRITE_ONCE(cpu_start_ccount, 0);
			return -EIO;
		}
	}
	return 0;
}
	for (i = 0; i < 2; ++i) {
		do
			ccount = get_ccount();
		while (!ccount);

		WRITE_ONCE(cpu_start_ccount, ccount);

		do {
			/*
			 * Pairs with the first two memws in the
			 * .Lboot_secondary.
			 */
			mb();
			ccount = READ_ONCE(cpu_start_ccount);
		} while (ccount && time_before(jiffies, timeout));

		if (ccount) {
			smp_call_function_single(0, mx_cpu_stop,
						 (void *)cpu, 1);
			WRITE_ONCE(cpu_start_ccount, 0);
			return -EIO;
		}
	}
{
	int ret = 0;

	if (cpu_asid_cache(cpu) == 0)
		cpu_asid_cache(cpu) = ASID_USER_FIRST;

	start_info.stack = (unsigned long)task_pt_regs(idle);
	wmb();

	pr_debug("%s: Calling wakeup_secondary(cpu:%d, idle:%p, sp: %08lx)\n",
			__func__, cpu, idle, start_info.stack);

	init_completion(&cpu_running);
	ret = boot_secondary(cpu, idle);
	if (ret == 0) {
		wait_for_completion_timeout(&cpu_running,
				msecs_to_jiffies(1000));
		if (!cpu_online(cpu))
			ret = -EIO;
	}
	while (time_before(jiffies, timeout)) {
		system_invalidate_dcache_range((unsigned long)&cpu_start_id,
					       sizeof(cpu_start_id));
		/* Pairs with the second memw in the cpu_restart */
		mb();
		if (READ_ONCE(cpu_start_id) == -cpu) {
			platform_cpu_kill(cpu);
			return;
		}
	}