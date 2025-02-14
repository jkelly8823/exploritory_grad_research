	VMXERR_INVALID_OPERAND_TO_INVEPT_INVVPID = 28,
};

enum vmx_l1d_flush_state {
	VMENTER_L1D_FLUSH_AUTO,
	VMENTER_L1D_FLUSH_NEVER,
	VMENTER_L1D_FLUSH_COND,
	VMENTER_L1D_FLUSH_ALWAYS,
	VMENTER_L1D_FLUSH_EPT_DISABLED,
	VMENTER_L1D_FLUSH_NOT_REQUIRED,
};

extern enum vmx_l1d_flush_state l1tf_vmx_mitigation;

#endif