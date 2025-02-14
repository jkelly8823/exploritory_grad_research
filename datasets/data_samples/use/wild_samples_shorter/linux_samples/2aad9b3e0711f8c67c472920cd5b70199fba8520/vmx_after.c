{
	BUILD_BUG_ON(ARRAY_SIZE(vmcs_field_to_offset_table) > SHRT_MAX);

	if (field >= ARRAY_SIZE(vmcs_field_to_offset_table))
		return -ENOENT;

	/*
	 * FIXME: Mitigation for CVE-2017-5753.  To be replaced with a
	 * generic mechanism.
	 */
	asm("lfence");

	if (vmcs_field_to_offset_table[field] == 0)
		return -ENOENT;

	return vmcs_field_to_offset_table[field];
}