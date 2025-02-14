	 * zap all shadow pages.
	 */
	if (unlikely(kvm_current_mmio_generation(kvm) == 0)) {
		printk_ratelimited(KERN_DEBUG "kvm: zapping shadow pages for mmio generation wraparound\n");
		kvm_mmu_invalidate_zap_all_pages(kvm);
	}
}
