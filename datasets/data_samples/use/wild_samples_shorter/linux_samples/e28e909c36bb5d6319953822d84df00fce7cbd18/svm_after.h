#define _UAPI__SVM_H

#define SVM_EXIT_READ_CR0      0x000
#define SVM_EXIT_READ_CR2      0x002
#define SVM_EXIT_READ_CR3      0x003
#define SVM_EXIT_READ_CR4      0x004
#define SVM_EXIT_READ_CR8      0x008
#define SVM_EXIT_WRITE_CR0     0x010
#define SVM_EXIT_WRITE_CR2     0x012
#define SVM_EXIT_WRITE_CR3     0x013
#define SVM_EXIT_WRITE_CR4     0x014
#define SVM_EXIT_WRITE_CR8     0x018
#define SVM_EXIT_READ_DR0      0x020

#define SVM_EXIT_REASONS \
	{ SVM_EXIT_READ_CR0,    "read_cr0" }, \
	{ SVM_EXIT_READ_CR2,    "read_cr2" }, \
	{ SVM_EXIT_READ_CR3,    "read_cr3" }, \
	{ SVM_EXIT_READ_CR4,    "read_cr4" }, \
	{ SVM_EXIT_READ_CR8,    "read_cr8" }, \
	{ SVM_EXIT_WRITE_CR0,   "write_cr0" }, \
	{ SVM_EXIT_WRITE_CR2,   "write_cr2" }, \
	{ SVM_EXIT_WRITE_CR3,   "write_cr3" }, \
	{ SVM_EXIT_WRITE_CR4,   "write_cr4" }, \
	{ SVM_EXIT_WRITE_CR8,   "write_cr8" }, \
	{ SVM_EXIT_READ_DR0,    "read_dr0" }, \
	{ SVM_EXIT_READ_DR1,    "read_dr1" }, \
	{ SVM_EXIT_READ_DR2,    "read_dr2" }, \
	{ SVM_EXIT_READ_DR3,    "read_dr3" }, \
	{ SVM_EXIT_READ_DR4,    "read_dr4" }, \
	{ SVM_EXIT_READ_DR5,    "read_dr5" }, \
	{ SVM_EXIT_READ_DR6,    "read_dr6" }, \
	{ SVM_EXIT_READ_DR7,    "read_dr7" }, \
	{ SVM_EXIT_WRITE_DR0,   "write_dr0" }, \
	{ SVM_EXIT_WRITE_DR1,   "write_dr1" }, \
	{ SVM_EXIT_WRITE_DR2,   "write_dr2" }, \
	{ SVM_EXIT_WRITE_DR3,   "write_dr3" }, \
	{ SVM_EXIT_WRITE_DR4,   "write_dr4" }, \
	{ SVM_EXIT_WRITE_DR5,   "write_dr5" }, \
	{ SVM_EXIT_WRITE_DR6,   "write_dr6" }, \
	{ SVM_EXIT_WRITE_DR7,   "write_dr7" }, \
	{ SVM_EXIT_EXCP_BASE + DE_VECTOR,       "DE excp" }, \
	{ SVM_EXIT_EXCP_BASE + DB_VECTOR,       "DB excp" }, \
	{ SVM_EXIT_EXCP_BASE + BP_VECTOR,       "BP excp" }, \
	{ SVM_EXIT_EXCP_BASE + OF_VECTOR,       "OF excp" }, \
	{ SVM_EXIT_EXCP_BASE + BR_VECTOR,       "BR excp" }, \
	{ SVM_EXIT_EXCP_BASE + UD_VECTOR,       "UD excp" }, \
	{ SVM_EXIT_EXCP_BASE + NM_VECTOR,       "NM excp" }, \
	{ SVM_EXIT_EXCP_BASE + DF_VECTOR,       "DF excp" }, \
	{ SVM_EXIT_EXCP_BASE + TS_VECTOR,       "TS excp" }, \
	{ SVM_EXIT_EXCP_BASE + NP_VECTOR,       "NP excp" }, \
	{ SVM_EXIT_EXCP_BASE + SS_VECTOR,       "SS excp" }, \
	{ SVM_EXIT_EXCP_BASE + GP_VECTOR,       "GP excp" }, \
	{ SVM_EXIT_EXCP_BASE + PF_VECTOR,       "PF excp" }, \
	{ SVM_EXIT_EXCP_BASE + MF_VECTOR,       "MF excp" }, \
	{ SVM_EXIT_EXCP_BASE + AC_VECTOR,       "AC excp" }, \
	{ SVM_EXIT_EXCP_BASE + MC_VECTOR,       "MC excp" }, \
	{ SVM_EXIT_EXCP_BASE + XM_VECTOR,       "XF excp" }, \
	{ SVM_EXIT_INTR,        "interrupt" }, \
	{ SVM_EXIT_NMI,         "nmi" }, \
	{ SVM_EXIT_SMI,         "smi" }, \
	{ SVM_EXIT_INIT,        "init" }, \
	{ SVM_EXIT_VINTR,       "vintr" }, \
	{ SVM_EXIT_CR0_SEL_WRITE, "cr0_sel_write" }, \
	{ SVM_EXIT_IDTR_READ,   "read_idtr" }, \
	{ SVM_EXIT_GDTR_READ,   "read_gdtr" }, \
	{ SVM_EXIT_LDTR_READ,   "read_ldtr" }, \
	{ SVM_EXIT_TR_READ,     "read_rt" }, \
	{ SVM_EXIT_IDTR_WRITE,  "write_idtr" }, \
	{ SVM_EXIT_GDTR_WRITE,  "write_gdtr" }, \
	{ SVM_EXIT_LDTR_WRITE,  "write_ldtr" }, \
	{ SVM_EXIT_TR_WRITE,    "write_rt" }, \
	{ SVM_EXIT_RDTSC,       "rdtsc" }, \
	{ SVM_EXIT_RDPMC,       "rdpmc" }, \
	{ SVM_EXIT_PUSHF,       "pushf" }, \
	{ SVM_EXIT_POPF,        "popf" }, \
	{ SVM_EXIT_CPUID,       "cpuid" }, \
	{ SVM_EXIT_RSM,         "rsm" }, \
	{ SVM_EXIT_IRET,        "iret" }, \
	{ SVM_EXIT_SWINT,       "swint" }, \
	{ SVM_EXIT_INVD,        "invd" }, \
	{ SVM_EXIT_PAUSE,       "pause" }, \
	{ SVM_EXIT_HLT,         "hlt" }, \
	{ SVM_EXIT_INVLPG,      "invlpg" }, \
	{ SVM_EXIT_IOIO,        "io" }, \
	{ SVM_EXIT_MSR,         "msr" }, \
	{ SVM_EXIT_TASK_SWITCH, "task_switch" }, \
	{ SVM_EXIT_FERR_FREEZE, "ferr_freeze" }, \
	{ SVM_EXIT_SHUTDOWN,    "shutdown" }, \
	{ SVM_EXIT_VMRUN,       "vmrun" }, \
	{ SVM_EXIT_VMMCALL,     "hypercall" }, \
	{ SVM_EXIT_VMLOAD,      "vmload" }, \
	{ SVM_EXIT_STGI,        "stgi" }, \
	{ SVM_EXIT_CLGI,        "clgi" }, \
	{ SVM_EXIT_SKINIT,      "skinit" }, \
	{ SVM_EXIT_RDTSCP,      "rdtscp" }, \
	{ SVM_EXIT_ICEBP,       "icebp" }, \
	{ SVM_EXIT_WBINVD,      "wbinvd" }, \
	{ SVM_EXIT_MONITOR,     "monitor" }, \
	{ SVM_EXIT_MWAIT,       "mwait" }, \
	{ SVM_EXIT_XSETBV,      "xsetbv" }, \
	{ SVM_EXIT_NPF,         "npf" }, \
	{ SVM_EXIT_AVIC_INCOMPLETE_IPI,		"avic_incomplete_ipi" }, \
	{ SVM_EXIT_AVIC_UNACCELERATED_ACCESS,   "avic_unaccelerated_access" }, \
	{ SVM_EXIT_ERR,         "invalid_guest_state" }


#endif /* _UAPI__SVM_H */