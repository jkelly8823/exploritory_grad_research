
static void record_steal_time(struct kvm_vcpu *vcpu)
{
	struct kvm_host_map map;
	struct kvm_steal_time *st;

	if (!(vcpu->arch.st.msr_val & KVM_MSR_ENABLED))
		return;

	/* -EAGAIN is returned in atomic context so we can just return. */
	if (kvm_map_gfn(vcpu, vcpu->arch.st.msr_val >> PAGE_SHIFT,
			&map, &vcpu->arch.st.cache, false))
		return;

	st = map.hva +
		offset_in_page(vcpu->arch.st.msr_val & KVM_STEAL_VALID_BITS);

	/*
	 * Doing a TLB flush here, on the guest's behalf, can avoid
	 * expensive IPIs.
	 */
	trace_kvm_pv_tlb_flush(vcpu->vcpu_id,
		st->preempted & KVM_VCPU_FLUSH_TLB);
	if (xchg(&st->preempted, 0) & KVM_VCPU_FLUSH_TLB)
		kvm_vcpu_flush_tlb(vcpu, false);

	vcpu->arch.st.steal.preempted = 0;

	if (st->version & 1)
		st->version += 1;  /* first time write, random junk */

	st->version += 1;

	smp_wmb();

	st->steal += current->sched_info.run_delay -
		vcpu->arch.st.last_steal;
	vcpu->arch.st.last_steal = current->sched_info.run_delay;

	smp_wmb();

	st->version += 1;

	kvm_unmap_gfn(vcpu, &map, &vcpu->arch.st.cache, true, false);
}

int kvm_set_msr_common(struct kvm_vcpu *vcpu, struct msr_data *msr_info)
{

static void kvm_steal_time_set_preempted(struct kvm_vcpu *vcpu)
{
	struct kvm_host_map map;
	struct kvm_steal_time *st;

	if (!(vcpu->arch.st.msr_val & KVM_MSR_ENABLED))
		return;

	if (vcpu->arch.st.steal.preempted)
		return;

	if (kvm_map_gfn(vcpu, vcpu->arch.st.msr_val >> PAGE_SHIFT, &map,
			&vcpu->arch.st.cache, true))
		return;

	st = map.hva +
		offset_in_page(vcpu->arch.st.msr_val & KVM_STEAL_VALID_BITS);

	st->preempted = vcpu->arch.st.steal.preempted = KVM_VCPU_PREEMPTED;

	kvm_unmap_gfn(vcpu, &map, &vcpu->arch.st.cache, true, true);
}

void kvm_arch_vcpu_put(struct kvm_vcpu *vcpu)
{