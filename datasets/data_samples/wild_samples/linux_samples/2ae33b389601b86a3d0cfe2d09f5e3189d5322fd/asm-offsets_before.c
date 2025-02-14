{
  DEFINE(TSK_ACTIVE_MM,		offsetof(struct task_struct, active_mm));
#ifdef CONFIG_CC_STACKPROTECTOR
  DEFINE(TSK_STACK_CANARY,	offsetof(struct task_struct, stack_canary));
#endif
  BLANK();
  DEFINE(TI_FLAGS,		offsetof(struct thread_info, flags));
  DEFINE(TI_PREEMPT,		offsetof(struct thread_info, preempt_count));
  DEFINE(TI_ADDR_LIMIT,		offsetof(struct thread_info, addr_limit));
  DEFINE(TI_TASK,		offsetof(struct thread_info, task));
  DEFINE(TI_EXEC_DOMAIN,	offsetof(struct thread_info, exec_domain));
  DEFINE(TI_CPU,		offsetof(struct thread_info, cpu));
  DEFINE(TI_CPU_DOMAIN,		offsetof(struct thread_info, cpu_domain));
  DEFINE(TI_CPU_SAVE,		offsetof(struct thread_info, cpu_context));
  DEFINE(TI_USED_CP,		offsetof(struct thread_info, used_cp));
  DEFINE(TI_TP_VALUE,		offsetof(struct thread_info, tp_value));
  DEFINE(TI_FPSTATE,		offsetof(struct thread_info, fpstate));
#ifdef CONFIG_VFP
  DEFINE(TI_VFPSTATE,		offsetof(struct thread_info, vfpstate));
#ifdef CONFIG_SMP
  DEFINE(VFP_CPU,		offsetof(union vfp_state, hard.cpu));
#endif
#endif
#ifdef CONFIG_ARM_THUMBEE
  DEFINE(TI_THUMBEE_STATE,	offsetof(struct thread_info, thumbee_state));
#endif
#ifdef CONFIG_IWMMXT
  DEFINE(TI_IWMMXT_STATE,	offsetof(struct thread_info, fpstate.iwmmxt));
#endif
#ifdef CONFIG_CRUNCH
  DEFINE(TI_CRUNCH_STATE,	offsetof(struct thread_info, crunchstate));
#endif
  BLANK();
  DEFINE(S_R0,			offsetof(struct pt_regs, ARM_r0));
  DEFINE(S_R1,			offsetof(struct pt_regs, ARM_r1));
  DEFINE(S_R2,			offsetof(struct pt_regs, ARM_r2));
  DEFINE(S_R3,			offsetof(struct pt_regs, ARM_r3));
  DEFINE(S_R4,			offsetof(struct pt_regs, ARM_r4));
  DEFINE(S_R5,			offsetof(struct pt_regs, ARM_r5));
  DEFINE(S_R6,			offsetof(struct pt_regs, ARM_r6));
  DEFINE(S_R7,			offsetof(struct pt_regs, ARM_r7));
  DEFINE(S_R8,			offsetof(struct pt_regs, ARM_r8));
  DEFINE(S_R9,			offsetof(struct pt_regs, ARM_r9));
  DEFINE(S_R10,			offsetof(struct pt_regs, ARM_r10));
  DEFINE(S_FP,			offsetof(struct pt_regs, ARM_fp));
  DEFINE(S_IP,			offsetof(struct pt_regs, ARM_ip));
  DEFINE(S_SP,			offsetof(struct pt_regs, ARM_sp));
  DEFINE(S_LR,			offsetof(struct pt_regs, ARM_lr));
  DEFINE(S_PC,			offsetof(struct pt_regs, ARM_pc));
  DEFINE(S_PSR,			offsetof(struct pt_regs, ARM_cpsr));
  DEFINE(S_OLD_R0,		offsetof(struct pt_regs, ARM_ORIG_r0));
  DEFINE(S_FRAME_SIZE,		sizeof(struct pt_regs));
  BLANK();
#ifdef CONFIG_CACHE_L2X0
  DEFINE(L2X0_R_PHY_BASE,	offsetof(struct l2x0_regs, phy_base));
  DEFINE(L2X0_R_AUX_CTRL,	offsetof(struct l2x0_regs, aux_ctrl));
  DEFINE(L2X0_R_TAG_LATENCY,	offsetof(struct l2x0_regs, tag_latency));
  DEFINE(L2X0_R_DATA_LATENCY,	offsetof(struct l2x0_regs, data_latency));
  DEFINE(L2X0_R_FILTER_START,	offsetof(struct l2x0_regs, filter_start));
  DEFINE(L2X0_R_FILTER_END,	offsetof(struct l2x0_regs, filter_end));
  DEFINE(L2X0_R_PREFETCH_CTRL,	offsetof(struct l2x0_regs, prefetch_ctrl));
  DEFINE(L2X0_R_PWR_CTRL,	offsetof(struct l2x0_regs, pwr_ctrl));
  BLANK();
#endif
#ifdef CONFIG_CPU_HAS_ASID
  DEFINE(MM_CONTEXT_ID,		offsetof(struct mm_struct, context.id));
  BLANK();
#endif
  DEFINE(VMA_VM_MM,		offsetof(struct vm_area_struct, vm_mm));
  DEFINE(VMA_VM_FLAGS,		offsetof(struct vm_area_struct, vm_flags));
  BLANK();
  DEFINE(VM_EXEC,	       	VM_EXEC);
  BLANK();
  DEFINE(PAGE_SZ,	       	PAGE_SIZE);
  BLANK();
  DEFINE(SYS_ERROR0,		0x9f0000);
  BLANK();
  DEFINE(SIZEOF_MACHINE_DESC,	sizeof(struct machine_desc));
  DEFINE(MACHINFO_TYPE,		offsetof(struct machine_desc, nr));
  DEFINE(MACHINFO_NAME,		offsetof(struct machine_desc, name));
  BLANK();
  DEFINE(PROC_INFO_SZ,		sizeof(struct proc_info_list));
  DEFINE(PROCINFO_INITFUNC,	offsetof(struct proc_info_list, __cpu_flush));
  DEFINE(PROCINFO_MM_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_mm_mmu_flags));
  DEFINE(PROCINFO_IO_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_io_mmu_flags));
  BLANK();
#ifdef MULTI_DABORT
  DEFINE(PROCESSOR_DABT_FUNC,	offsetof(struct processor, _data_abort));
#endif
#ifdef MULTI_PABORT
  DEFINE(PROCESSOR_PABT_FUNC,	offsetof(struct processor, _prefetch_abort));
#endif
#ifdef MULTI_CPU
  DEFINE(CPU_SLEEP_SIZE,	offsetof(struct processor, suspend_size));
  DEFINE(CPU_DO_SUSPEND,	offsetof(struct processor, do_suspend));
  DEFINE(CPU_DO_RESUME,		offsetof(struct processor, do_resume));
#endif
#ifdef MULTI_CACHE
  DEFINE(CACHE_FLUSH_KERN_ALL,	offsetof(struct cpu_cache_fns, flush_kern_all));
#endif
  BLANK();
  DEFINE(DMA_BIDIRECTIONAL,	DMA_BIDIRECTIONAL);
  DEFINE(DMA_TO_DEVICE,		DMA_TO_DEVICE);
  DEFINE(DMA_FROM_DEVICE,	DMA_FROM_DEVICE);
#ifdef CONFIG_KVM_ARM_HOST
  DEFINE(VCPU_KVM,		offsetof(struct kvm_vcpu, kvm));
  DEFINE(VCPU_MIDR,		offsetof(struct kvm_vcpu, arch.midr));
  DEFINE(VCPU_CP15,		offsetof(struct kvm_vcpu, arch.cp15));
  DEFINE(VCPU_VFP_GUEST,	offsetof(struct kvm_vcpu, arch.vfp_guest));
  DEFINE(VCPU_VFP_HOST,		offsetof(struct kvm_vcpu, arch.vfp_host));
  DEFINE(VCPU_REGS,		offsetof(struct kvm_vcpu, arch.regs));
  DEFINE(VCPU_USR_REGS,		offsetof(struct kvm_vcpu, arch.regs.usr_regs));
  DEFINE(VCPU_SVC_REGS,		offsetof(struct kvm_vcpu, arch.regs.svc_regs));
  DEFINE(VCPU_ABT_REGS,		offsetof(struct kvm_vcpu, arch.regs.abt_regs));
  DEFINE(VCPU_UND_REGS,		offsetof(struct kvm_vcpu, arch.regs.und_regs));
  DEFINE(VCPU_IRQ_REGS,		offsetof(struct kvm_vcpu, arch.regs.irq_regs));
  DEFINE(VCPU_FIQ_REGS,		offsetof(struct kvm_vcpu, arch.regs.fiq_regs));
  DEFINE(VCPU_PC,		offsetof(struct kvm_vcpu, arch.regs.usr_regs.ARM_pc));
  DEFINE(VCPU_CPSR,		offsetof(struct kvm_vcpu, arch.regs.usr_regs.ARM_cpsr));
  DEFINE(VCPU_IRQ_LINES,	offsetof(struct kvm_vcpu, arch.irq_lines));
  DEFINE(VCPU_HSR,		offsetof(struct kvm_vcpu, arch.hsr));
  DEFINE(VCPU_HxFAR,		offsetof(struct kvm_vcpu, arch.hxfar));
  DEFINE(VCPU_HPFAR,		offsetof(struct kvm_vcpu, arch.hpfar));
  DEFINE(VCPU_HYP_PC,		offsetof(struct kvm_vcpu, arch.hyp_pc));
#ifdef CONFIG_KVM_ARM_VGIC
  DEFINE(VCPU_VGIC_CPU,		offsetof(struct kvm_vcpu, arch.vgic_cpu));
  DEFINE(VGIC_CPU_HCR,		offsetof(struct vgic_cpu, vgic_hcr));
  DEFINE(VGIC_CPU_VMCR,		offsetof(struct vgic_cpu, vgic_vmcr));
  DEFINE(VGIC_CPU_MISR,		offsetof(struct vgic_cpu, vgic_misr));
  DEFINE(VGIC_CPU_EISR,		offsetof(struct vgic_cpu, vgic_eisr));
  DEFINE(VGIC_CPU_ELRSR,	offsetof(struct vgic_cpu, vgic_elrsr));
  DEFINE(VGIC_CPU_APR,		offsetof(struct vgic_cpu, vgic_apr));
  DEFINE(VGIC_CPU_LR,		offsetof(struct vgic_cpu, vgic_lr));
  DEFINE(VGIC_CPU_NR_LR,	offsetof(struct vgic_cpu, nr_lr));
#ifdef CONFIG_KVM_ARM_TIMER
  DEFINE(VCPU_TIMER_CNTV_CTL,	offsetof(struct kvm_vcpu, arch.timer_cpu.cntv_ctl));
  DEFINE(VCPU_TIMER_CNTV_CVAL,	offsetof(struct kvm_vcpu, arch.timer_cpu.cntv_cval));
  DEFINE(KVM_TIMER_CNTVOFF,	offsetof(struct kvm, arch.timer.cntvoff));
  DEFINE(KVM_TIMER_ENABLED,	offsetof(struct kvm, arch.timer.enabled));
#endif
  DEFINE(KVM_VGIC_VCTRL,	offsetof(struct kvm, arch.vgic.vctrl_base));
#endif
  DEFINE(KVM_VTTBR,		offsetof(struct kvm, arch.vttbr));
#endif
  return 0; 
}