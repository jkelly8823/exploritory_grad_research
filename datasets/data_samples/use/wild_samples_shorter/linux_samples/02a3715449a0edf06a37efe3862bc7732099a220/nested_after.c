	/* If SMI is not intercepted, ignore guest SMI intercept as well  */
	if (!intercept_smi)
		vmcb_clr_intercept(c, INTERCEPT_SMI);

	vmcb_set_intercept(c, INTERCEPT_VMLOAD);
	vmcb_set_intercept(c, INTERCEPT_VMSAVE);
}

static void copy_vmcb_control_area(struct vmcb_control_area *dst,
				   struct vmcb_control_area *from)

static void nested_vmcb02_prepare_control(struct vcpu_svm *svm)
{
	const u32 int_ctl_vmcb01_bits =
		V_INTR_MASKING_MASK | V_GIF_MASK | V_GIF_ENABLE_MASK;

	const u32 int_ctl_vmcb12_bits = V_TPR_MASK | V_IRQ_INJECTION_BITS_MASK;

	struct kvm_vcpu *vcpu = &svm->vcpu;

	/*
	 * Filled at exit: exit_code, exit_code_hi, exit_info_1, exit_info_2,
		vcpu->arch.l1_tsc_offset + svm->nested.ctl.tsc_offset;

	svm->vmcb->control.int_ctl             =
		(svm->nested.ctl.int_ctl & int_ctl_vmcb12_bits) |
		(svm->vmcb01.ptr->control.int_ctl & int_ctl_vmcb01_bits);

	svm->vmcb->control.virt_ext            = svm->nested.ctl.virt_ext;
	svm->vmcb->control.int_vector          = svm->nested.ctl.int_vector;
	svm->vmcb->control.int_state           = svm->nested.ctl.int_state;