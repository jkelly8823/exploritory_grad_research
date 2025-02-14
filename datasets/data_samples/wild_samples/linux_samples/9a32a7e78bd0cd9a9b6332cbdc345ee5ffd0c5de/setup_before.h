enum l1d_flush_type {
	L1D_FLUSH_NONE		= 0x1,
	L1D_FLUSH_FALLBACK	= 0x2,
	L1D_FLUSH_ORI		= 0x4,
	L1D_FLUSH_MTTRIG	= 0x8,
};

void setup_rfi_flush(enum l1d_flush_type, bool enable);
void setup_entry_flush(bool enable);
void setup_uaccess_flush(bool enable);
void do_rfi_flush_fixups(enum l1d_flush_type types);
#ifdef CONFIG_PPC_BARRIER_NOSPEC
void setup_barrier_nospec(void);
#else
static inline void setup_barrier_nospec(void) { };
#endif
void do_entry_flush_fixups(enum l1d_flush_type types);
void do_barrier_nospec_fixups(bool enable);
extern bool barrier_nospec_enabled;

#ifdef CONFIG_PPC_BARRIER_NOSPEC
void do_barrier_nospec_fixups_range(bool enable, void *start, void *end);
#else
static inline void do_barrier_nospec_fixups_range(bool enable, void *start, void *end) { };
#endif

#ifdef CONFIG_PPC_FSL_BOOK3E
void setup_spectre_v2(void);
#else
static inline void setup_spectre_v2(void) {};
#endif
void do_btb_flush_fixups(void);

#endif /* !__ASSEMBLY__ */

#endif	/* _ASM_POWERPC_SETUP_H */
