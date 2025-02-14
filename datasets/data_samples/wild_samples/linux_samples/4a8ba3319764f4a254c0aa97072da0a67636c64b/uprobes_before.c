{
	struct pt_regs *regs = task_pt_regs(tsk);

	if (regs->int_code != UPROBE_TRAP_NR)
		return true;
	return false;
}

int arch_uprobe_post_xol(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	int fixup = probe_get_fixup_type(auprobe->insn);
	struct uprobe_task *utask = current->utask;

	clear_tsk_thread_flag(current, TIF_UPROBE_SINGLESTEP);
	update_cr_regs(current);
	psw_bits(regs->psw).r = auprobe->saved_per;
	regs->int_code = auprobe->saved_int_code;

	if (fixup & FIXUP_PSW_NORMAL)
		regs->psw.addr += utask->vaddr - utask->xol_vaddr;
	if (fixup & FIXUP_RETURN_REGISTER) {
		int reg = (auprobe->insn[0] & 0xf0) >> 4;

		regs->gprs[reg] += utask->vaddr - utask->xol_vaddr;
	}
	if (fixup & FIXUP_BRANCH_NOT_TAKEN) {
		int ilen = insn_length(auprobe->insn[0] >> 8);

		if (regs->psw.addr - utask->xol_vaddr == ilen)
			regs->psw.addr = utask->vaddr + ilen;
	}
	/* If per tracing was active generate trap */
	if (regs->psw.mask & PSW_MASK_PER)
		do_per_trap(regs);
	return 0;
}

int arch_uprobe_exception_notify(struct notifier_block *self, unsigned long val,
				 void *data)
{
	struct die_args *args = data;
	struct pt_regs *regs = args->regs;

	if (!user_mode(regs))
		return NOTIFY_DONE;
	if (regs->int_code & 0x200) /* Trap during transaction */
		return NOTIFY_DONE;
	switch (val) {
	case DIE_BPT:
		if (uprobe_pre_sstep_notifier(regs))
			return NOTIFY_STOP;
		break;
	case DIE_SSTEP:
		if (uprobe_post_sstep_notifier(regs))
			return NOTIFY_STOP;
	default:
		break;
	}
	return NOTIFY_DONE;
}

void arch_uprobe_abort_xol(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	clear_thread_flag(TIF_UPROBE_SINGLESTEP);
	regs->int_code = auprobe->saved_int_code;
	regs->psw.addr = current->utask->vaddr;
}

unsigned long arch_uretprobe_hijack_return_addr(unsigned long trampoline,
						struct pt_regs *regs)
{
	unsigned long orig;

	orig = regs->gprs[14];
	regs->gprs[14] = trampoline;
	return orig;
}

/* Instruction Emulation */

static void adjust_psw_addr(psw_t *psw, unsigned long len)
{
	psw->addr = __rewind_psw(*psw, -len);
}

#define EMU_ILLEGAL_OP		1
#define EMU_SPECIFICATION	2
#define EMU_ADDRESSING		3

#define emu_load_ril(ptr, output)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	__typeof__(*(ptr)) input;			\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (get_user(input, ptr))			\
		__rc = EMU_ADDRESSING;			\
	else						\
		*(output) = input;			\
	__rc;						\
})

#define emu_store_ril(ptr, input)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (put_user(*(input), ptr))		\
		__rc = EMU_ADDRESSING;			\
	__rc;						\
})

#define emu_cmp_ril(regs, ptr, cmp)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	__typeof__(*(ptr)) input;			\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (get_user(input, ptr))			\
		__rc = EMU_ADDRESSING;			\
	else if (input > *(cmp))			\
		psw_bits((regs)->psw).cc = 1;		\
	else if (input < *(cmp))			\
		psw_bits((regs)->psw).cc = 2;		\
	else						\
		psw_bits((regs)->psw).cc = 0;		\
	__rc;						\
})

struct insn_ril {
	u8 opc0;
	u8 reg	: 4;
	u8 opc1 : 4;
	s32 disp;
} __packed;

union split_register {
	u64 u64;
	u32 u32[2];
	u16 u16[4];
	s64 s64;
	s32 s32[2];
	s16 s16[4];
};

/*
 * pc relative instructions are emulated, since parameters may not be
 * accessible from the xol area due to range limitations.
 */
static void handle_insn_ril(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	union split_register *rx;
	struct insn_ril *insn;
	unsigned int ilen;
	void *uptr;
	int rc = 0;

	insn = (struct insn_ril *) &auprobe->insn;
	rx = (union split_register *) &regs->gprs[insn->reg];
	uptr = (void *)(regs->psw.addr + (insn->disp * 2));
	ilen = insn_length(insn->opc0);

	switch (insn->opc0) {
	case 0xc0:
		switch (insn->opc1) {
		case 0x00: /* larl */
			rx->u64 = (unsigned long)uptr;
			break;
		}
		break;
	case 0xc4:
		switch (insn->opc1) {
		case 0x02: /* llhrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x04: /* lghrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u64);
			break;
		case 0x05: /* lhrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x06: /* llghrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u64);
			break;
		case 0x08: /* lgrl */
			rc = emu_load_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* lgfrl */
			rc = emu_load_ril((s32 __user *)uptr, &rx->u64);
			break;
		case 0x0d: /* lrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		case 0x0e: /* llgfrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* sthrl */
			rc = emu_store_ril((u16 __user *)uptr, &rx->u16[3]);
			break;
		case 0x0b: /* stgrl */
			rc = emu_store_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* strl */
			rc = emu_store_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	case 0xc6:
		switch (insn->opc1) {
		case 0x02: /* pfdrl */
			if (!test_facility(34))
				rc = EMU_ILLEGAL_OP;
			break;
		case 0x04: /* cghrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s64);
			break;
		case 0x05: /* chrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s32[1]);
			break;
		case 0x06: /* clghrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* clhrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x08: /* cgrl */
			rc = emu_cmp_ril(regs, (s64 __user *)uptr, &rx->s64);
			break;
		case 0x0a: /* clgrl */
			rc = emu_cmp_ril(regs, (u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* cgfrl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s64);
			break;
		case 0x0d: /* crl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s32[1]);
			break;
		case 0x0e: /* clgfrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* clrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	}
	adjust_psw_addr(&regs->psw, ilen);
	switch (rc) {
	case EMU_ILLEGAL_OP:
		regs->int_code = ilen << 16 | 0x0001;
		do_report_trap(regs, SIGILL, ILL_ILLOPC, NULL);
		break;
	case EMU_SPECIFICATION:
		regs->int_code = ilen << 16 | 0x0006;
		do_report_trap(regs, SIGILL, ILL_ILLOPC , NULL);
		break;
	case EMU_ADDRESSING:
		regs->int_code = ilen << 16 | 0x0005;
		do_report_trap(regs, SIGSEGV, SEGV_MAPERR, NULL);
		break;
	}
}
{
	clear_thread_flag(TIF_UPROBE_SINGLESTEP);
	regs->int_code = auprobe->saved_int_code;
	regs->psw.addr = current->utask->vaddr;
}

unsigned long arch_uretprobe_hijack_return_addr(unsigned long trampoline,
						struct pt_regs *regs)
{
	unsigned long orig;

	orig = regs->gprs[14];
	regs->gprs[14] = trampoline;
	return orig;
}

/* Instruction Emulation */

static void adjust_psw_addr(psw_t *psw, unsigned long len)
{
	psw->addr = __rewind_psw(*psw, -len);
}

#define EMU_ILLEGAL_OP		1
#define EMU_SPECIFICATION	2
#define EMU_ADDRESSING		3

#define emu_load_ril(ptr, output)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	__typeof__(*(ptr)) input;			\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (get_user(input, ptr))			\
		__rc = EMU_ADDRESSING;			\
	else						\
		*(output) = input;			\
	__rc;						\
})

#define emu_store_ril(ptr, input)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (put_user(*(input), ptr))		\
		__rc = EMU_ADDRESSING;			\
	__rc;						\
})

#define emu_cmp_ril(regs, ptr, cmp)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	__typeof__(*(ptr)) input;			\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (get_user(input, ptr))			\
		__rc = EMU_ADDRESSING;			\
	else if (input > *(cmp))			\
		psw_bits((regs)->psw).cc = 1;		\
	else if (input < *(cmp))			\
		psw_bits((regs)->psw).cc = 2;		\
	else						\
		psw_bits((regs)->psw).cc = 0;		\
	__rc;						\
})

struct insn_ril {
	u8 opc0;
	u8 reg	: 4;
	u8 opc1 : 4;
	s32 disp;
} __packed;

union split_register {
	u64 u64;
	u32 u32[2];
	u16 u16[4];
	s64 s64;
	s32 s32[2];
	s16 s16[4];
};

/*
 * pc relative instructions are emulated, since parameters may not be
 * accessible from the xol area due to range limitations.
 */
static void handle_insn_ril(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	union split_register *rx;
	struct insn_ril *insn;
	unsigned int ilen;
	void *uptr;
	int rc = 0;

	insn = (struct insn_ril *) &auprobe->insn;
	rx = (union split_register *) &regs->gprs[insn->reg];
	uptr = (void *)(regs->psw.addr + (insn->disp * 2));
	ilen = insn_length(insn->opc0);

	switch (insn->opc0) {
	case 0xc0:
		switch (insn->opc1) {
		case 0x00: /* larl */
			rx->u64 = (unsigned long)uptr;
			break;
		}
		break;
	case 0xc4:
		switch (insn->opc1) {
		case 0x02: /* llhrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x04: /* lghrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u64);
			break;
		case 0x05: /* lhrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x06: /* llghrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u64);
			break;
		case 0x08: /* lgrl */
			rc = emu_load_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* lgfrl */
			rc = emu_load_ril((s32 __user *)uptr, &rx->u64);
			break;
		case 0x0d: /* lrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		case 0x0e: /* llgfrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* sthrl */
			rc = emu_store_ril((u16 __user *)uptr, &rx->u16[3]);
			break;
		case 0x0b: /* stgrl */
			rc = emu_store_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* strl */
			rc = emu_store_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	case 0xc6:
		switch (insn->opc1) {
		case 0x02: /* pfdrl */
			if (!test_facility(34))
				rc = EMU_ILLEGAL_OP;
			break;
		case 0x04: /* cghrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s64);
			break;
		case 0x05: /* chrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s32[1]);
			break;
		case 0x06: /* clghrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* clhrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x08: /* cgrl */
			rc = emu_cmp_ril(regs, (s64 __user *)uptr, &rx->s64);
			break;
		case 0x0a: /* clgrl */
			rc = emu_cmp_ril(regs, (u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* cgfrl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s64);
			break;
		case 0x0d: /* crl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s32[1]);
			break;
		case 0x0e: /* clgfrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* clrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	}
	adjust_psw_addr(&regs->psw, ilen);
	switch (rc) {
	case EMU_ILLEGAL_OP:
		regs->int_code = ilen << 16 | 0x0001;
		do_report_trap(regs, SIGILL, ILL_ILLOPC, NULL);
		break;
	case EMU_SPECIFICATION:
		regs->int_code = ilen << 16 | 0x0006;
		do_report_trap(regs, SIGILL, ILL_ILLOPC , NULL);
		break;
	case EMU_ADDRESSING:
		regs->int_code = ilen << 16 | 0x0005;
		do_report_trap(regs, SIGSEGV, SEGV_MAPERR, NULL);
		break;
	}
}

bool arch_uprobe_skip_sstep(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	if ((psw_bits(regs->psw).eaba == PSW_AMODE_24BIT) ||
	    ((psw_bits(regs->psw).eaba == PSW_AMODE_31BIT) &&
	     !is_compat_task())) {
		regs->psw.addr = __rewind_psw(regs->psw, UPROBE_SWBP_INSN_SIZE);
		do_report_trap(regs, SIGILL, ILL_ILLADR, NULL);
		return true;
	}
	if (probe_is_insn_relative_long(auprobe->insn)) {
		handle_insn_ril(auprobe, regs);
		return true;
	}
	return false;
}
{
	psw->addr = __rewind_psw(*psw, -len);
}

#define EMU_ILLEGAL_OP		1
#define EMU_SPECIFICATION	2
#define EMU_ADDRESSING		3

#define emu_load_ril(ptr, output)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	__typeof__(*(ptr)) input;			\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (get_user(input, ptr))			\
		__rc = EMU_ADDRESSING;			\
	else						\
		*(output) = input;			\
	__rc;						\
})

#define emu_store_ril(ptr, input)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (put_user(*(input), ptr))		\
		__rc = EMU_ADDRESSING;			\
	__rc;						\
})

#define emu_cmp_ril(regs, ptr, cmp)			\
({							\
	unsigned int mask = sizeof(*(ptr)) - 1;		\
	__typeof__(*(ptr)) input;			\
	int __rc = 0;					\
							\
	if (!test_facility(34))				\
		__rc = EMU_ILLEGAL_OP;			\
	else if ((u64 __force)ptr & mask)		\
		__rc = EMU_SPECIFICATION;		\
	else if (get_user(input, ptr))			\
		__rc = EMU_ADDRESSING;			\
	else if (input > *(cmp))			\
		psw_bits((regs)->psw).cc = 1;		\
	else if (input < *(cmp))			\
		psw_bits((regs)->psw).cc = 2;		\
	else						\
		psw_bits((regs)->psw).cc = 0;		\
	__rc;						\
})

struct insn_ril {
	u8 opc0;
	u8 reg	: 4;
	u8 opc1 : 4;
	s32 disp;
} __packed;

union split_register {
	u64 u64;
	u32 u32[2];
	u16 u16[4];
	s64 s64;
	s32 s32[2];
	s16 s16[4];
};

/*
 * pc relative instructions are emulated, since parameters may not be
 * accessible from the xol area due to range limitations.
 */
static void handle_insn_ril(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	union split_register *rx;
	struct insn_ril *insn;
	unsigned int ilen;
	void *uptr;
	int rc = 0;

	insn = (struct insn_ril *) &auprobe->insn;
	rx = (union split_register *) &regs->gprs[insn->reg];
	uptr = (void *)(regs->psw.addr + (insn->disp * 2));
	ilen = insn_length(insn->opc0);

	switch (insn->opc0) {
	case 0xc0:
		switch (insn->opc1) {
		case 0x00: /* larl */
			rx->u64 = (unsigned long)uptr;
			break;
		}
		break;
	case 0xc4:
		switch (insn->opc1) {
		case 0x02: /* llhrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x04: /* lghrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u64);
			break;
		case 0x05: /* lhrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x06: /* llghrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u64);
			break;
		case 0x08: /* lgrl */
			rc = emu_load_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* lgfrl */
			rc = emu_load_ril((s32 __user *)uptr, &rx->u64);
			break;
		case 0x0d: /* lrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		case 0x0e: /* llgfrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* sthrl */
			rc = emu_store_ril((u16 __user *)uptr, &rx->u16[3]);
			break;
		case 0x0b: /* stgrl */
			rc = emu_store_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* strl */
			rc = emu_store_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	case 0xc6:
		switch (insn->opc1) {
		case 0x02: /* pfdrl */
			if (!test_facility(34))
				rc = EMU_ILLEGAL_OP;
			break;
		case 0x04: /* cghrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s64);
			break;
		case 0x05: /* chrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s32[1]);
			break;
		case 0x06: /* clghrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* clhrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x08: /* cgrl */
			rc = emu_cmp_ril(regs, (s64 __user *)uptr, &rx->s64);
			break;
		case 0x0a: /* clgrl */
			rc = emu_cmp_ril(regs, (u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* cgfrl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s64);
			break;
		case 0x0d: /* crl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s32[1]);
			break;
		case 0x0e: /* clgfrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* clrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	}
	adjust_psw_addr(&regs->psw, ilen);
	switch (rc) {
	case EMU_ILLEGAL_OP:
		regs->int_code = ilen << 16 | 0x0001;
		do_report_trap(regs, SIGILL, ILL_ILLOPC, NULL);
		break;
	case EMU_SPECIFICATION:
		regs->int_code = ilen << 16 | 0x0006;
		do_report_trap(regs, SIGILL, ILL_ILLOPC , NULL);
		break;
	case EMU_ADDRESSING:
		regs->int_code = ilen << 16 | 0x0005;
		do_report_trap(regs, SIGSEGV, SEGV_MAPERR, NULL);
		break;
	}
}

bool arch_uprobe_skip_sstep(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	if ((psw_bits(regs->psw).eaba == PSW_AMODE_24BIT) ||
	    ((psw_bits(regs->psw).eaba == PSW_AMODE_31BIT) &&
	     !is_compat_task())) {
		regs->psw.addr = __rewind_psw(regs->psw, UPROBE_SWBP_INSN_SIZE);
		do_report_trap(regs, SIGILL, ILL_ILLADR, NULL);
		return true;
	}
	if (probe_is_insn_relative_long(auprobe->insn)) {
		handle_insn_ril(auprobe, regs);
		return true;
	}
	return false;
}
union split_register {
	u64 u64;
	u32 u32[2];
	u16 u16[4];
	s64 s64;
	s32 s32[2];
	s16 s16[4];
};

/*
 * pc relative instructions are emulated, since parameters may not be
 * accessible from the xol area due to range limitations.
 */
static void handle_insn_ril(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	union split_register *rx;
	struct insn_ril *insn;
	unsigned int ilen;
	void *uptr;
	int rc = 0;

	insn = (struct insn_ril *) &auprobe->insn;
	rx = (union split_register *) &regs->gprs[insn->reg];
	uptr = (void *)(regs->psw.addr + (insn->disp * 2));
	ilen = insn_length(insn->opc0);

	switch (insn->opc0) {
	case 0xc0:
		switch (insn->opc1) {
		case 0x00: /* larl */
			rx->u64 = (unsigned long)uptr;
			break;
		}
		break;
	case 0xc4:
		switch (insn->opc1) {
		case 0x02: /* llhrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x04: /* lghrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u64);
			break;
		case 0x05: /* lhrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x06: /* llghrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u64);
			break;
		case 0x08: /* lgrl */
			rc = emu_load_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* lgfrl */
			rc = emu_load_ril((s32 __user *)uptr, &rx->u64);
			break;
		case 0x0d: /* lrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		case 0x0e: /* llgfrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* sthrl */
			rc = emu_store_ril((u16 __user *)uptr, &rx->u16[3]);
			break;
		case 0x0b: /* stgrl */
			rc = emu_store_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* strl */
			rc = emu_store_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	case 0xc6:
		switch (insn->opc1) {
		case 0x02: /* pfdrl */
			if (!test_facility(34))
				rc = EMU_ILLEGAL_OP;
			break;
		case 0x04: /* cghrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s64);
			break;
		case 0x05: /* chrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s32[1]);
			break;
		case 0x06: /* clghrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* clhrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x08: /* cgrl */
			rc = emu_cmp_ril(regs, (s64 __user *)uptr, &rx->s64);
			break;
		case 0x0a: /* clgrl */
			rc = emu_cmp_ril(regs, (u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* cgfrl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s64);
			break;
		case 0x0d: /* crl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s32[1]);
			break;
		case 0x0e: /* clgfrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* clrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	}
	adjust_psw_addr(&regs->psw, ilen);
	switch (rc) {
	case EMU_ILLEGAL_OP:
		regs->int_code = ilen << 16 | 0x0001;
		do_report_trap(regs, SIGILL, ILL_ILLOPC, NULL);
		break;
	case EMU_SPECIFICATION:
		regs->int_code = ilen << 16 | 0x0006;
		do_report_trap(regs, SIGILL, ILL_ILLOPC , NULL);
		break;
	case EMU_ADDRESSING:
		regs->int_code = ilen << 16 | 0x0005;
		do_report_trap(regs, SIGSEGV, SEGV_MAPERR, NULL);
		break;
	}
}
		switch (insn->opc1) {
		case 0x02: /* llhrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x04: /* lghrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u64);
			break;
		case 0x05: /* lhrl */
			rc = emu_load_ril((s16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x06: /* llghrl */
			rc = emu_load_ril((u16 __user *)uptr, &rx->u64);
			break;
		case 0x08: /* lgrl */
			rc = emu_load_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* lgfrl */
			rc = emu_load_ril((s32 __user *)uptr, &rx->u64);
			break;
		case 0x0d: /* lrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		case 0x0e: /* llgfrl */
			rc = emu_load_ril((u32 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* sthrl */
			rc = emu_store_ril((u16 __user *)uptr, &rx->u16[3]);
			break;
		case 0x0b: /* stgrl */
			rc = emu_store_ril((u64 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* strl */
			rc = emu_store_ril((u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	case 0xc6:
		switch (insn->opc1) {
		case 0x02: /* pfdrl */
			if (!test_facility(34))
				rc = EMU_ILLEGAL_OP;
			break;
		case 0x04: /* cghrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s64);
			break;
		case 0x05: /* chrl */
			rc = emu_cmp_ril(regs, (s16 __user *)uptr, &rx->s32[1]);
			break;
		case 0x06: /* clghrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u64);
			break;
		case 0x07: /* clhrl */
			rc = emu_cmp_ril(regs, (u16 __user *)uptr, &rx->u32[1]);
			break;
		case 0x08: /* cgrl */
			rc = emu_cmp_ril(regs, (s64 __user *)uptr, &rx->s64);
			break;
		case 0x0a: /* clgrl */
			rc = emu_cmp_ril(regs, (u64 __user *)uptr, &rx->u64);
			break;
		case 0x0c: /* cgfrl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s64);
			break;
		case 0x0d: /* crl */
			rc = emu_cmp_ril(regs, (s32 __user *)uptr, &rx->s32[1]);
			break;
		case 0x0e: /* clgfrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u64);
			break;
		case 0x0f: /* clrl */
			rc = emu_cmp_ril(regs, (u32 __user *)uptr, &rx->u32[1]);
			break;
		}
		break;
	}
	adjust_psw_addr(&regs->psw, ilen);
	switch (rc) {
	case EMU_ILLEGAL_OP:
		regs->int_code = ilen << 16 | 0x0001;
		do_report_trap(regs, SIGILL, ILL_ILLOPC, NULL);
		break;
	case EMU_SPECIFICATION:
		regs->int_code = ilen << 16 | 0x0006;
		do_report_trap(regs, SIGILL, ILL_ILLOPC , NULL);
		break;
	case EMU_ADDRESSING:
		regs->int_code = ilen << 16 | 0x0005;
		do_report_trap(regs, SIGSEGV, SEGV_MAPERR, NULL);
		break;
	}
}

bool arch_uprobe_skip_sstep(struct arch_uprobe *auprobe, struct pt_regs *regs)
{
	if ((psw_bits(regs->psw).eaba == PSW_AMODE_24BIT) ||
	    ((psw_bits(regs->psw).eaba == PSW_AMODE_31BIT) &&
	     !is_compat_task())) {
		regs->psw.addr = __rewind_psw(regs->psw, UPROBE_SWBP_INSN_SIZE);
		do_report_trap(regs, SIGILL, ILL_ILLADR, NULL);
		return true;
	}
	if (probe_is_insn_relative_long(auprobe->insn)) {
		handle_insn_ril(auprobe, regs);
		return true;
	}
	return false;
}