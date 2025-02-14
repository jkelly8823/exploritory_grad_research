		if (test_bit(KVM_ARM_VCPU_EL1_32BIT, vcpu->arch.features)) {
			if (!cpu_has_32bit_el1())
				return -EINVAL;
			cpu_reset = &default_regs_reset32;
		} else {
			cpu_reset = &default_regs_reset;
		}