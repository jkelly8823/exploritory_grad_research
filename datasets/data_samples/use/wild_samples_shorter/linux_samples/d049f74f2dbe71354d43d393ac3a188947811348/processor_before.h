	regs->loadrs = 0;									\
	regs->r8 = get_dumpable(current->mm);	/* set "don't zap registers" flag */		\
	regs->r12 = new_sp - 16;	/* allocate 16 byte scratch area */			\
	if (unlikely(!get_dumpable(current->mm))) {							\
		/*										\
		 * Zap scratch regs to avoid leaking bits between processes with different	\
		 * uid/privileges.								\
		 */										\