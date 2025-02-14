	if (g->int_ctl & V_INTR_MASKING_MASK) {
		/* We only want the cr8 intercept bits of L1 */
		vmcb_clr_intercept(c, INTERCEPT_CR8_READ);
		vmcb_clr_intercept(c, INTERCEPT_CR8_WRITE);

		/*
		 * Once running L2 with HF_VINTR_MASK, EFLAGS.IF does not
		 * affect any interrupt we may want to inject; therefore,
		 * interrupt window vmexits are irrelevant to L0.
		 */
		vmcb_clr_intercept(c, INTERCEPT_VINTR);
	}

	/* We don't want to see VMMCALLs from a nested guest */
	vmcb_clr_intercept(c, INTERCEPT_VMMCALL);

	for (i = 0; i < MAX_INTERCEPT; i++)
		c->intercepts[i] |= g->intercepts[i];

	/* If SMI is not intercepted, ignore guest SMI intercept as well  */
	if (!intercept_smi)
		vmcb_clr_intercept(c, INTERCEPT_SMI);

	vmcb_set_intercept(c, INTERCEPT_VMLOAD);
	vmcb_set_intercept(c, INTERCEPT_VMSAVE);
}

static void copy_vmcb_control_area(struct vmcb_control_area *dst,
				   struct vmcb_control_area *from)
{
	unsigned int i;

	for (i = 0; i < MAX_INTERCEPT; i++)
		dst->intercepts[i] = from->intercepts[i];

	dst->iopm_base_pa         = from->iopm_base_pa;
	dst->msrpm_base_pa        = from->msrpm_base_pa;
	dst->tsc_offset           = from->tsc_offset;
	/* asid not copied, it is handled manually for svm->vmcb.  */
	dst->tlb_ctl              = from->tlb_ctl;
	dst->int_ctl              = from->int_ctl;
	dst->int_vector           = from->int_vector;
	dst->int_state            = from->int_state;
	dst->exit_code            = from->exit_code;
	dst->exit_code_hi         = from->exit_code_hi;
	dst->exit_info_1          = from->exit_info_1;
	dst->exit_info_2          = from->exit_info_2;
	dst->exit_int_info        = from->exit_int_info;
	dst->exit_int_info_err    = from->exit_int_info_err;
	dst->nested_ctl           = from->nested_ctl;
	dst->event_inj            = from->event_inj;
	dst->event_inj_err        = from->event_inj_err;
	dst->nested_cr3           = from->nested_cr3;
	dst->virt_ext              = from->virt_ext;
	dst->pause_filter_count   = from->pause_filter_count;
	dst->pause_filter_thresh  = from->pause_filter_thresh;
}

static bool nested_svm_vmrun_msrpm(struct vcpu_svm *svm)
{
	/*
	 * This function merges the msr permission bitmaps of kvm and the
	 * nested vmcb. It is optimized in that it only merges the parts where
	 * the kvm msr permission bitmap may contain zero bits
	 */
	int i;

	if (!(vmcb_is_intercept(&svm->nested.ctl, INTERCEPT_MSR_PROT)))
		return true;

	for (i = 0; i < MSRPM_OFFSETS; i++) {
		u32 value, p;
		u64 offset;

		if (msrpm_offsets[i] == 0xffffffff)
			break;

		p      = msrpm_offsets[i];
		offset = svm->nested.ctl.msrpm_base_pa + (p * 4);

		if (kvm_vcpu_read_guest(&svm->vcpu, offset, &value, 4))
			return false;

		svm->nested.msrpm[p] = svm->msrpm[p] | value;
	}
	if (unlikely(new_vmcb12 || vmcb_is_dirty(vmcb12, VMCB_DR))) {
		svm->vmcb->save.dr7 = vmcb12->save.dr7 | DR7_FIXED_1;
		svm->vcpu.arch.dr6  = vmcb12->save.dr6 | DR6_ACTIVE_LOW;
		vmcb_mark_dirty(svm->vmcb, VMCB_DR);
	}
}

static void nested_vmcb02_prepare_control(struct vcpu_svm *svm)
{
	const u32 int_ctl_vmcb01_bits =
		V_INTR_MASKING_MASK | V_GIF_MASK | V_GIF_ENABLE_MASK;

	const u32 int_ctl_vmcb12_bits = V_TPR_MASK | V_IRQ_INJECTION_BITS_MASK;

	struct kvm_vcpu *vcpu = &svm->vcpu;

	/*
	 * Filled at exit: exit_code, exit_code_hi, exit_info_1, exit_info_2,
	 * exit_int_info, exit_int_info_err, next_rip, insn_len, insn_bytes.
	 */

	/*
	 * Also covers avic_vapic_bar, avic_backing_page, avic_logical_id,
	 * avic_physical_id.
	 */
	WARN_ON(kvm_apicv_activated(svm->vcpu.kvm));

	/* Copied from vmcb01.  msrpm_base can be overwritten later.  */
	svm->vmcb->control.nested_ctl = svm->vmcb01.ptr->control.nested_ctl;
	svm->vmcb->control.iopm_base_pa = svm->vmcb01.ptr->control.iopm_base_pa;
	svm->vmcb->control.msrpm_base_pa = svm->vmcb01.ptr->control.msrpm_base_pa;

	/* Done at vmrun: asid.  */

	/* Also overwritten later if necessary.  */
	svm->vmcb->control.tlb_ctl = TLB_CONTROL_DO_NOTHING;

	/* nested_cr3.  */
	if (nested_npt_enabled(svm))
		nested_svm_init_mmu_context(vcpu);

	svm->vmcb->control.tsc_offset = vcpu->arch.tsc_offset =
		vcpu->arch.l1_tsc_offset + svm->nested.ctl.tsc_offset;

	svm->vmcb->control.int_ctl             =
		(svm->nested.ctl.int_ctl & int_ctl_vmcb12_bits) |
		(svm->vmcb01.ptr->control.int_ctl & int_ctl_vmcb01_bits);

	svm->vmcb->control.virt_ext            = svm->nested.ctl.virt_ext;
	svm->vmcb->control.int_vector          = svm->nested.ctl.int_vector;
	svm->vmcb->control.int_state           = svm->nested.ctl.int_state;
	svm->vmcb->control.event_inj           = svm->nested.ctl.event_inj;
	svm->vmcb->control.event_inj_err       = svm->nested.ctl.event_inj_err;

	svm->vmcb->control.pause_filter_count  = svm->nested.ctl.pause_filter_count;
	svm->vmcb->control.pause_filter_thresh = svm->nested.ctl.pause_filter_thresh;

	nested_svm_transition_tlb_flush(vcpu);

	/* Enter Guest-Mode */
	enter_guest_mode(vcpu);

	/*
	 * Merge guest and host intercepts - must be called with vcpu in
	 * guest-mode to take effect.
	 */
	recalc_intercepts(svm);
}
{
	const u32 int_ctl_vmcb01_bits =
		V_INTR_MASKING_MASK | V_GIF_MASK | V_GIF_ENABLE_MASK;

	const u32 int_ctl_vmcb12_bits = V_TPR_MASK | V_IRQ_INJECTION_BITS_MASK;

	struct kvm_vcpu *vcpu = &svm->vcpu;

	/*
	 * Filled at exit: exit_code, exit_code_hi, exit_info_1, exit_info_2,
	 * exit_int_info, exit_int_info_err, next_rip, insn_len, insn_bytes.
	 */

	/*
	 * Also covers avic_vapic_bar, avic_backing_page, avic_logical_id,
	 * avic_physical_id.
	 */
	WARN_ON(kvm_apicv_activated(svm->vcpu.kvm));

	/* Copied from vmcb01.  msrpm_base can be overwritten later.  */
	svm->vmcb->control.nested_ctl = svm->vmcb01.ptr->control.nested_ctl;
	svm->vmcb->control.iopm_base_pa = svm->vmcb01.ptr->control.iopm_base_pa;
	svm->vmcb->control.msrpm_base_pa = svm->vmcb01.ptr->control.msrpm_base_pa;

	/* Done at vmrun: asid.  */

	/* Also overwritten later if necessary.  */
	svm->vmcb->control.tlb_ctl = TLB_CONTROL_DO_NOTHING;

	/* nested_cr3.  */
	if (nested_npt_enabled(svm))
		nested_svm_init_mmu_context(vcpu);

	svm->vmcb->control.tsc_offset = vcpu->arch.tsc_offset =
		vcpu->arch.l1_tsc_offset + svm->nested.ctl.tsc_offset;

	svm->vmcb->control.int_ctl             =
		(svm->nested.ctl.int_ctl & int_ctl_vmcb12_bits) |
		(svm->vmcb01.ptr->control.int_ctl & int_ctl_vmcb01_bits);

	svm->vmcb->control.virt_ext            = svm->nested.ctl.virt_ext;
	svm->vmcb->control.int_vector          = svm->nested.ctl.int_vector;
	svm->vmcb->control.int_state           = svm->nested.ctl.int_state;
	svm->vmcb->control.event_inj           = svm->nested.ctl.event_inj;
	svm->vmcb->control.event_inj_err       = svm->nested.ctl.event_inj_err;

	svm->vmcb->control.pause_filter_count  = svm->nested.ctl.pause_filter_count;
	svm->vmcb->control.pause_filter_thresh = svm->nested.ctl.pause_filter_thresh;

	nested_svm_transition_tlb_flush(vcpu);

	/* Enter Guest-Mode */
	enter_guest_mode(vcpu);

	/*
	 * Merge guest and host intercepts - must be called with vcpu in
	 * guest-mode to take effect.
	 */
	recalc_intercepts(svm);
}

static void nested_svm_copy_common_state(struct vmcb *from_vmcb, struct vmcb *to_vmcb)
{
	/*
	 * Some VMCB state is shared between L1 and L2 and thus has to be
	 * moved at the time of nested vmrun and vmexit.
	 *
	 * VMLOAD/VMSAVE state would also belong in this category, but KVM
	 * always performs VMLOAD and VMSAVE from the VMCB01.
	 */
	to_vmcb->save.spec_ctrl = from_vmcb->save.spec_ctrl;
}

int enter_svm_guest_mode(struct kvm_vcpu *vcpu, u64 vmcb12_gpa,
			 struct vmcb *vmcb12)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	int ret;

	trace_kvm_nested_vmrun(svm->vmcb->save.rip, vmcb12_gpa,
			       vmcb12->save.rip,
			       vmcb12->control.int_ctl,
			       vmcb12->control.event_inj,
			       vmcb12->control.nested_ctl);

	trace_kvm_nested_intercepts(vmcb12->control.intercepts[INTERCEPT_CR] & 0xffff,
				    vmcb12->control.intercepts[INTERCEPT_CR] >> 16,
				    vmcb12->control.intercepts[INTERCEPT_EXCEPTION],
				    vmcb12->control.intercepts[INTERCEPT_WORD3],
				    vmcb12->control.intercepts[INTERCEPT_WORD4],
				    vmcb12->control.intercepts[INTERCEPT_WORD5]);


	svm->nested.vmcb12_gpa = vmcb12_gpa;

	WARN_ON(svm->vmcb == svm->nested.vmcb02.ptr);

	nested_svm_copy_common_state(svm->vmcb01.ptr, svm->nested.vmcb02.ptr);

	svm_switch_vmcb(svm, &svm->nested.vmcb02);
	nested_vmcb02_prepare_control(svm);
	nested_vmcb02_prepare_save(svm, vmcb12);

	ret = nested_svm_load_cr3(&svm->vcpu, vmcb12->save.cr3,
				  nested_npt_enabled(svm), true);
	if (ret)
		return ret;

	if (!npt_enabled)
		vcpu->arch.mmu->inject_page_fault = svm_inject_page_fault_nested;

	svm_set_gif(svm, true);

	return 0;
}

int nested_svm_vmrun(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	int ret;
	struct vmcb *vmcb12;
	struct kvm_host_map map;
	u64 vmcb12_gpa;

	if (!svm->nested.hsave_msr) {
		kvm_inject_gp(vcpu, 0);
		return 1;
	}