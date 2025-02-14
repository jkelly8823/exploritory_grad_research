struct kvm_lapic {
	unsigned long base_address;
	struct kvm_io_device dev;
	struct kvm_timer lapic_timer;
	u32 divide_count;
	struct kvm_vcpu *vcpu;
	bool irr_pending;
	/* Number of bits set in ISR. */
	s16 isr_count;
	/* The highest vector set in ISR; if -1 - invalid, must scan ISR. */
	int highest_isr_cache;
	/**
	 * APIC register page.  The layout matches the register layout seen by
	 * the guest 1:1, because it is accessed by the vmx microcode.
	 * Note: Only one register, the TPR, is used by the microcode.
	 */
	void *regs;
	gpa_t vapic_addr;
	struct gfn_to_hva_cache vapic_cache;
	unsigned long pending_events;
	unsigned int sipi_vector;
};
int kvm_create_lapic(struct kvm_vcpu *vcpu);
void kvm_free_lapic(struct kvm_vcpu *vcpu);

int kvm_apic_has_interrupt(struct kvm_vcpu *vcpu);
int kvm_apic_accept_pic_intr(struct kvm_vcpu *vcpu);
int kvm_get_apic_interrupt(struct kvm_vcpu *vcpu);
void kvm_apic_accept_events(struct kvm_vcpu *vcpu);
void kvm_lapic_reset(struct kvm_vcpu *vcpu);
u64 kvm_lapic_get_cr8(struct kvm_vcpu *vcpu);
void kvm_lapic_set_tpr(struct kvm_vcpu *vcpu, unsigned long cr8);
void kvm_lapic_set_eoi(struct kvm_vcpu *vcpu);
void kvm_lapic_set_base(struct kvm_vcpu *vcpu, u64 value);
u64 kvm_lapic_get_base(struct kvm_vcpu *vcpu);
void kvm_apic_set_version(struct kvm_vcpu *vcpu);

void kvm_apic_update_tmr(struct kvm_vcpu *vcpu, u32 *tmr);
void kvm_apic_update_irr(struct kvm_vcpu *vcpu, u32 *pir);
int kvm_apic_match_physical_addr(struct kvm_lapic *apic, u16 dest);
int kvm_apic_match_logical_addr(struct kvm_lapic *apic, u8 mda);
int kvm_apic_set_irq(struct kvm_vcpu *vcpu, struct kvm_lapic_irq *irq,
		unsigned long *dest_map);
int kvm_apic_local_deliver(struct kvm_lapic *apic, int lvt_type);

bool kvm_irq_delivery_to_apic_fast(struct kvm *kvm, struct kvm_lapic *src,
		struct kvm_lapic_irq *irq, int *r, unsigned long *dest_map);

u64 kvm_get_apic_base(struct kvm_vcpu *vcpu);
void kvm_set_apic_base(struct kvm_vcpu *vcpu, u64 data);
void kvm_apic_post_state_restore(struct kvm_vcpu *vcpu,
		struct kvm_lapic_state *s);
int kvm_lapic_find_highest_irr(struct kvm_vcpu *vcpu);

u64 kvm_get_lapic_tscdeadline_msr(struct kvm_vcpu *vcpu);
void kvm_set_lapic_tscdeadline_msr(struct kvm_vcpu *vcpu, u64 data);

void kvm_apic_write_nodecode(struct kvm_vcpu *vcpu, u32 offset);
void kvm_apic_set_eoi_accelerated(struct kvm_vcpu *vcpu, int vector);

int kvm_lapic_set_vapic_addr(struct kvm_vcpu *vcpu, gpa_t vapic_addr);
void kvm_lapic_sync_from_vapic(struct kvm_vcpu *vcpu);
void kvm_lapic_sync_to_vapic(struct kvm_vcpu *vcpu);

int kvm_x2apic_msr_write(struct kvm_vcpu *vcpu, u32 msr, u64 data);
int kvm_x2apic_msr_read(struct kvm_vcpu *vcpu, u32 msr, u64 *data);

int kvm_hv_vapic_msr_write(struct kvm_vcpu *vcpu, u32 msr, u64 data);
int kvm_hv_vapic_msr_read(struct kvm_vcpu *vcpu, u32 msr, u64 *data);

static inline bool kvm_hv_vapic_assist_page_enabled(struct kvm_vcpu *vcpu)
{
	return vcpu->arch.hv_vapic & HV_X64_MSR_APIC_ASSIST_PAGE_ENABLE;
}
struct kvm_lapic {
	unsigned long base_address;
	struct kvm_io_device dev;
	struct kvm_timer lapic_timer;
	u32 divide_count;
	struct kvm_vcpu *vcpu;
	bool irr_pending;
	/* Number of bits set in ISR. */
	s16 isr_count;
	/* The highest vector set in ISR; if -1 - invalid, must scan ISR. */
	int highest_isr_cache;
	/**
	 * APIC register page.  The layout matches the register layout seen by
	 * the guest 1:1, because it is accessed by the vmx microcode.
	 * Note: Only one register, the TPR, is used by the microcode.
	 */
	void *regs;
	gpa_t vapic_addr;
	struct gfn_to_hva_cache vapic_cache;
	unsigned long pending_events;
	unsigned int sipi_vector;
};
int kvm_create_lapic(struct kvm_vcpu *vcpu);
void kvm_free_lapic(struct kvm_vcpu *vcpu);

int kvm_apic_has_interrupt(struct kvm_vcpu *vcpu);
int kvm_apic_accept_pic_intr(struct kvm_vcpu *vcpu);
int kvm_get_apic_interrupt(struct kvm_vcpu *vcpu);
void kvm_apic_accept_events(struct kvm_vcpu *vcpu);
void kvm_lapic_reset(struct kvm_vcpu *vcpu);
u64 kvm_lapic_get_cr8(struct kvm_vcpu *vcpu);
void kvm_lapic_set_tpr(struct kvm_vcpu *vcpu, unsigned long cr8);
void kvm_lapic_set_eoi(struct kvm_vcpu *vcpu);
void kvm_lapic_set_base(struct kvm_vcpu *vcpu, u64 value);
u64 kvm_lapic_get_base(struct kvm_vcpu *vcpu);
void kvm_apic_set_version(struct kvm_vcpu *vcpu);

void kvm_apic_update_tmr(struct kvm_vcpu *vcpu, u32 *tmr);
void kvm_apic_update_irr(struct kvm_vcpu *vcpu, u32 *pir);
int kvm_apic_match_physical_addr(struct kvm_lapic *apic, u16 dest);
int kvm_apic_match_logical_addr(struct kvm_lapic *apic, u8 mda);
int kvm_apic_set_irq(struct kvm_vcpu *vcpu, struct kvm_lapic_irq *irq,
		unsigned long *dest_map);
int kvm_apic_local_deliver(struct kvm_lapic *apic, int lvt_type);

bool kvm_irq_delivery_to_apic_fast(struct kvm *kvm, struct kvm_lapic *src,
		struct kvm_lapic_irq *irq, int *r, unsigned long *dest_map);

u64 kvm_get_apic_base(struct kvm_vcpu *vcpu);
void kvm_set_apic_base(struct kvm_vcpu *vcpu, u64 data);
void kvm_apic_post_state_restore(struct kvm_vcpu *vcpu,
		struct kvm_lapic_state *s);
int kvm_lapic_find_highest_irr(struct kvm_vcpu *vcpu);

u64 kvm_get_lapic_tscdeadline_msr(struct kvm_vcpu *vcpu);
void kvm_set_lapic_tscdeadline_msr(struct kvm_vcpu *vcpu, u64 data);

void kvm_apic_write_nodecode(struct kvm_vcpu *vcpu, u32 offset);
void kvm_apic_set_eoi_accelerated(struct kvm_vcpu *vcpu, int vector);

int kvm_lapic_set_vapic_addr(struct kvm_vcpu *vcpu, gpa_t vapic_addr);
void kvm_lapic_sync_from_vapic(struct kvm_vcpu *vcpu);
void kvm_lapic_sync_to_vapic(struct kvm_vcpu *vcpu);

int kvm_x2apic_msr_write(struct kvm_vcpu *vcpu, u32 msr, u64 data);
int kvm_x2apic_msr_read(struct kvm_vcpu *vcpu, u32 msr, u64 *data);

int kvm_hv_vapic_msr_write(struct kvm_vcpu *vcpu, u32 msr, u64 data);
int kvm_hv_vapic_msr_read(struct kvm_vcpu *vcpu, u32 msr, u64 *data);

static inline bool kvm_hv_vapic_assist_page_enabled(struct kvm_vcpu *vcpu)
{
	return vcpu->arch.hv_vapic & HV_X64_MSR_APIC_ASSIST_PAGE_ENABLE;
}