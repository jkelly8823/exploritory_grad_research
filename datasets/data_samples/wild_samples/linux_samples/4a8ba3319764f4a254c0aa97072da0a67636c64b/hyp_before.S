/*
 * Copyright (C) 2012,2013 - ARM Ltd
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/memory.h>
#include <asm/asm-offsets.h>
#include <asm/debug-monitors.h>
#include <asm/fpsimdmacros.h>
#include <asm/kvm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>

#define CPU_GP_REG_OFFSET(x)	(CPU_GP_REGS + x)
#define CPU_XREG_OFFSET(x)	CPU_GP_REG_OFFSET(CPU_USER_PT_REGS + 8*x)
#define CPU_SPSR_OFFSET(x)	CPU_GP_REG_OFFSET(CPU_SPSR + 8*x)
#define CPU_SYSREG_OFFSET(x)	(CPU_SYSREGS + 8*x)

	.text
	.pushsection	.hyp.text, "ax"
	.align	PAGE_SHIFT

.macro save_common_regs
	// x2: base address for cpu context
	// x3: tmp register

	add	x3, x2, #CPU_XREG_OFFSET(19)
	stp	x19, x20, [x3]
	stp	x21, x22, [x3, #16]
	stp	x23, x24, [x3, #32]
	stp	x25, x26, [x3, #48]
	stp	x27, x28, [x3, #64]
	stp	x29, lr, [x3, #80]

	mrs	x19, sp_el0
	mrs	x20, elr_el2		// EL1 PC
	mrs	x21, spsr_el2		// EL1 pstate

	stp	x19, x20, [x3, #96]
	str	x21, [x3, #112]

	mrs	x22, sp_el1
	mrs	x23, elr_el1
	mrs	x24, spsr_el1

	str	x22, [x2, #CPU_GP_REG_OFFSET(CPU_SP_EL1)]
	str	x23, [x2, #CPU_GP_REG_OFFSET(CPU_ELR_EL1)]
	str	x24, [x2, #CPU_SPSR_OFFSET(KVM_SPSR_EL1)]
.endm

.macro restore_common_regs
	// x2: base address for cpu context
	// x3: tmp register

	ldr	x22, [x2, #CPU_GP_REG_OFFSET(CPU_SP_EL1)]
	ldr	x23, [x2, #CPU_GP_REG_OFFSET(CPU_ELR_EL1)]
	ldr	x24, [x2, #CPU_SPSR_OFFSET(KVM_SPSR_EL1)]

	msr	sp_el1, x22
	msr	elr_el1, x23
	msr	spsr_el1, x24

	add	x3, x2, #CPU_XREG_OFFSET(31)    // SP_EL0
	ldp	x19, x20, [x3]
	ldr	x21, [x3, #16]

	msr	sp_el0, x19
	msr	elr_el2, x20 				// EL1 PC
	msr	spsr_el2, x21 				// EL1 pstate

	add	x3, x2, #CPU_XREG_OFFSET(19)
	ldp	x19, x20, [x3]
	ldp	x21, x22, [x3, #16]
	ldp	x23, x24, [x3, #32]
	ldp	x25, x26, [x3, #48]
	ldp	x27, x28, [x3, #64]
	ldp	x29, lr, [x3, #80]
.endm

.macro save_host_regs
	save_common_regs
.endm

.macro restore_host_regs
	restore_common_regs
.endm

.macro save_fpsimd
	// x2: cpu context address
	// x3, x4: tmp regs
	add	x3, x2, #CPU_GP_REG_OFFSET(CPU_FP_REGS)
	fpsimd_save x3, 4
.endm

.macro restore_fpsimd
	// x2: cpu context address
	// x3, x4: tmp regs
	add	x3, x2, #CPU_GP_REG_OFFSET(CPU_FP_REGS)
	fpsimd_restore x3, 4
.endm

.macro save_guest_regs
	// x0 is the vcpu address
	// x1 is the return code, do not corrupt!
	// x2 is the cpu context
	// x3 is a tmp register
	// Guest's x0-x3 are on the stack

	// Compute base to save registers
	add	x3, x2, #CPU_XREG_OFFSET(4)
	stp	x4, x5, [x3]
	stp	x6, x7, [x3, #16]
	stp	x8, x9, [x3, #32]
	stp	x10, x11, [x3, #48]
	stp	x12, x13, [x3, #64]
	stp	x14, x15, [x3, #80]
	stp	x16, x17, [x3, #96]
	str	x18, [x3, #112]

	pop	x6, x7			// x2, x3
	pop	x4, x5			// x0, x1

	add	x3, x2, #CPU_XREG_OFFSET(0)
	stp	x4, x5, [x3]
	stp	x6, x7, [x3, #16]

	save_common_regs
.endm

.macro restore_guest_regs
	// x0 is the vcpu address.
	// x2 is the cpu context
	// x3 is a tmp register

	// Prepare x0-x3 for later restore
	add	x3, x2, #CPU_XREG_OFFSET(0)
	ldp	x4, x5, [x3]
	ldp	x6, x7, [x3, #16]
	push	x4, x5		// Push x0-x3 on the stack
	push	x6, x7

	// x4-x18
	ldp	x4, x5, [x3, #32]
	ldp	x6, x7, [x3, #48]
	ldp	x8, x9, [x3, #64]
	ldp	x10, x11, [x3, #80]
	ldp	x12, x13, [x3, #96]
	ldp	x14, x15, [x3, #112]
	ldp	x16, x17, [x3, #128]
	ldr	x18, [x3, #144]

	// x19-x29, lr, sp*, elr*, spsr*
	restore_common_regs

	// Last bits of the 64bit state
	pop	x2, x3
	pop	x0, x1

	// Do not touch any register after this!
.endm

/*
 * Macros to perform system register save/restore.
 *
 * Ordering here is absolutely critical, and must be kept consistent
 * in {save,restore}_sysregs, {save,restore}_guest_32bit_state,
 * and in kvm_asm.h.
 *
 * In other words, don't touch any of these unless you know what
 * you are doing.
 */
.macro save_sysregs
	// x2: base address for cpu context
	// x3: tmp register

	add	x3, x2, #CPU_SYSREG_OFFSET(MPIDR_EL1)

	mrs	x4,	vmpidr_el2
	mrs	x5,	csselr_el1
	mrs	x6,	sctlr_el1
	mrs	x7,	actlr_el1
	mrs	x8,	cpacr_el1
	mrs	x9,	ttbr0_el1
	mrs	x10,	ttbr1_el1
	mrs	x11,	tcr_el1
	mrs	x12,	esr_el1
	mrs	x13, 	afsr0_el1
	mrs	x14,	afsr1_el1
	mrs	x15,	far_el1
	mrs	x16,	mair_el1
	mrs	x17,	vbar_el1
	mrs	x18,	contextidr_el1
	mrs	x19,	tpidr_el0
	mrs	x20,	tpidrro_el0
	mrs	x21,	tpidr_el1
	mrs	x22, 	amair_el1
	mrs	x23, 	cntkctl_el1
	mrs	x24,	par_el1
	mrs	x25,	mdscr_el1

	stp	x4, x5, [x3]
	stp	x6, x7, [x3, #16]
	stp	x8, x9, [x3, #32]
	stp	x10, x11, [x3, #48]
	stp	x12, x13, [x3, #64]
	stp	x14, x15, [x3, #80]
	stp	x16, x17, [x3, #96]
	stp	x18, x19, [x3, #112]
	stp	x20, x21, [x3, #128]
	stp	x22, x23, [x3, #144]
	stp	x24, x25, [x3, #160]
.endm

.macro save_debug
	// x2: base address for cpu context
	// x3: tmp register

	mrs	x26, id_aa64dfr0_el1
	ubfx	x24, x26, #12, #4	// Extract BRPs
	ubfx	x25, x26, #20, #4	// Extract WRPs
	mov	w26, #15
	sub	w24, w26, w24		// How many BPs to skip
	sub	w25, w26, w25		// How many WPs to skip

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGBCR0_EL1)

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26
1:
	mrs	x20, dbgbcr15_el1
	mrs	x19, dbgbcr14_el1
	mrs	x18, dbgbcr13_el1
	mrs	x17, dbgbcr12_el1
	mrs	x16, dbgbcr11_el1
	mrs	x15, dbgbcr10_el1
	mrs	x14, dbgbcr9_el1
	mrs	x13, dbgbcr8_el1
	mrs	x12, dbgbcr7_el1
	mrs	x11, dbgbcr6_el1
	mrs	x10, dbgbcr5_el1
	mrs	x9, dbgbcr4_el1
	mrs	x8, dbgbcr3_el1
	mrs	x7, dbgbcr2_el1
	mrs	x6, dbgbcr1_el1
	mrs	x5, dbgbcr0_el1

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26

1:
	str	x20, [x3, #(15 * 8)]
	str	x19, [x3, #(14 * 8)]
	str	x18, [x3, #(13 * 8)]
	str	x17, [x3, #(12 * 8)]
	str	x16, [x3, #(11 * 8)]
	str	x15, [x3, #(10 * 8)]
	str	x14, [x3, #(9 * 8)]
	str	x13, [x3, #(8 * 8)]
	str	x12, [x3, #(7 * 8)]
	str	x11, [x3, #(6 * 8)]
	str	x10, [x3, #(5 * 8)]
	str	x9, [x3, #(4 * 8)]
	str	x8, [x3, #(3 * 8)]
	str	x7, [x3, #(2 * 8)]
	str	x6, [x3, #(1 * 8)]
	str	x5, [x3, #(0 * 8)]

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGBVR0_EL1)

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26
1:
	mrs	x20, dbgbvr15_el1
	mrs	x19, dbgbvr14_el1
	mrs	x18, dbgbvr13_el1
	mrs	x17, dbgbvr12_el1
	mrs	x16, dbgbvr11_el1
	mrs	x15, dbgbvr10_el1
	mrs	x14, dbgbvr9_el1
	mrs	x13, dbgbvr8_el1
	mrs	x12, dbgbvr7_el1
	mrs	x11, dbgbvr6_el1
	mrs	x10, dbgbvr5_el1
	mrs	x9, dbgbvr4_el1
	mrs	x8, dbgbvr3_el1
	mrs	x7, dbgbvr2_el1
	mrs	x6, dbgbvr1_el1
	mrs	x5, dbgbvr0_el1

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26

1:
	str	x20, [x3, #(15 * 8)]
	str	x19, [x3, #(14 * 8)]
	str	x18, [x3, #(13 * 8)]
	str	x17, [x3, #(12 * 8)]
	str	x16, [x3, #(11 * 8)]
	str	x15, [x3, #(10 * 8)]
	str	x14, [x3, #(9 * 8)]
	str	x13, [x3, #(8 * 8)]
	str	x12, [x3, #(7 * 8)]
	str	x11, [x3, #(6 * 8)]
	str	x10, [x3, #(5 * 8)]
	str	x9, [x3, #(4 * 8)]
	str	x8, [x3, #(3 * 8)]
	str	x7, [x3, #(2 * 8)]
	str	x6, [x3, #(1 * 8)]
	str	x5, [x3, #(0 * 8)]

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGWCR0_EL1)

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26
1:
	mrs	x20, dbgwcr15_el1
	mrs	x19, dbgwcr14_el1
	mrs	x18, dbgwcr13_el1
	mrs	x17, dbgwcr12_el1
	mrs	x16, dbgwcr11_el1
	mrs	x15, dbgwcr10_el1
	mrs	x14, dbgwcr9_el1
	mrs	x13, dbgwcr8_el1
	mrs	x12, dbgwcr7_el1
	mrs	x11, dbgwcr6_el1
	mrs	x10, dbgwcr5_el1
	mrs	x9, dbgwcr4_el1
	mrs	x8, dbgwcr3_el1
	mrs	x7, dbgwcr2_el1
	mrs	x6, dbgwcr1_el1
	mrs	x5, dbgwcr0_el1

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26

1:
	str	x20, [x3, #(15 * 8)]
	str	x19, [x3, #(14 * 8)]
	str	x18, [x3, #(13 * 8)]
	str	x17, [x3, #(12 * 8)]
	str	x16, [x3, #(11 * 8)]
	str	x15, [x3, #(10 * 8)]
	str	x14, [x3, #(9 * 8)]
	str	x13, [x3, #(8 * 8)]
	str	x12, [x3, #(7 * 8)]
	str	x11, [x3, #(6 * 8)]
	str	x10, [x3, #(5 * 8)]
	str	x9, [x3, #(4 * 8)]
	str	x8, [x3, #(3 * 8)]
	str	x7, [x3, #(2 * 8)]
	str	x6, [x3, #(1 * 8)]
	str	x5, [x3, #(0 * 8)]

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGWVR0_EL1)

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26
1:
	mrs	x20, dbgwvr15_el1
	mrs	x19, dbgwvr14_el1
	mrs	x18, dbgwvr13_el1
	mrs	x17, dbgwvr12_el1
	mrs	x16, dbgwvr11_el1
	mrs	x15, dbgwvr10_el1
	mrs	x14, dbgwvr9_el1
	mrs	x13, dbgwvr8_el1
	mrs	x12, dbgwvr7_el1
	mrs	x11, dbgwvr6_el1
	mrs	x10, dbgwvr5_el1
	mrs	x9, dbgwvr4_el1
	mrs	x8, dbgwvr3_el1
	mrs	x7, dbgwvr2_el1
	mrs	x6, dbgwvr1_el1
	mrs	x5, dbgwvr0_el1

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26

1:
	str	x20, [x3, #(15 * 8)]
	str	x19, [x3, #(14 * 8)]
	str	x18, [x3, #(13 * 8)]
	str	x17, [x3, #(12 * 8)]
	str	x16, [x3, #(11 * 8)]
	str	x15, [x3, #(10 * 8)]
	str	x14, [x3, #(9 * 8)]
	str	x13, [x3, #(8 * 8)]
	str	x12, [x3, #(7 * 8)]
	str	x11, [x3, #(6 * 8)]
	str	x10, [x3, #(5 * 8)]
	str	x9, [x3, #(4 * 8)]
	str	x8, [x3, #(3 * 8)]
	str	x7, [x3, #(2 * 8)]
	str	x6, [x3, #(1 * 8)]
	str	x5, [x3, #(0 * 8)]

	mrs	x21, mdccint_el1
	str	x21, [x2, #CPU_SYSREG_OFFSET(MDCCINT_EL1)]
.endm

.macro restore_sysregs
	// x2: base address for cpu context
	// x3: tmp register

	add	x3, x2, #CPU_SYSREG_OFFSET(MPIDR_EL1)

	ldp	x4, x5, [x3]
	ldp	x6, x7, [x3, #16]
	ldp	x8, x9, [x3, #32]
	ldp	x10, x11, [x3, #48]
	ldp	x12, x13, [x3, #64]
	ldp	x14, x15, [x3, #80]
	ldp	x16, x17, [x3, #96]
	ldp	x18, x19, [x3, #112]
	ldp	x20, x21, [x3, #128]
	ldp	x22, x23, [x3, #144]
	ldp	x24, x25, [x3, #160]

	msr	vmpidr_el2,	x4
	msr	csselr_el1,	x5
	msr	sctlr_el1,	x6
	msr	actlr_el1,	x7
	msr	cpacr_el1,	x8
	msr	ttbr0_el1,	x9
	msr	ttbr1_el1,	x10
	msr	tcr_el1,	x11
	msr	esr_el1,	x12
	msr	afsr0_el1,	x13
	msr	afsr1_el1,	x14
	msr	far_el1,	x15
	msr	mair_el1,	x16
	msr	vbar_el1,	x17
	msr	contextidr_el1,	x18
	msr	tpidr_el0,	x19
	msr	tpidrro_el0,	x20
	msr	tpidr_el1,	x21
	msr	amair_el1,	x22
	msr	cntkctl_el1,	x23
	msr	par_el1,	x24
	msr	mdscr_el1,	x25
.endm

.macro restore_debug
	// x2: base address for cpu context
	// x3: tmp register

	mrs	x26, id_aa64dfr0_el1
	ubfx	x24, x26, #12, #4	// Extract BRPs
	ubfx	x25, x26, #20, #4	// Extract WRPs
	mov	w26, #15
	sub	w24, w26, w24		// How many BPs to skip
	sub	w25, w26, w25		// How many WPs to skip

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGBCR0_EL1)

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26
1:
	ldr	x20, [x3, #(15 * 8)]
	ldr	x19, [x3, #(14 * 8)]
	ldr	x18, [x3, #(13 * 8)]
	ldr	x17, [x3, #(12 * 8)]
	ldr	x16, [x3, #(11 * 8)]
	ldr	x15, [x3, #(10 * 8)]
	ldr	x14, [x3, #(9 * 8)]
	ldr	x13, [x3, #(8 * 8)]
	ldr	x12, [x3, #(7 * 8)]
	ldr	x11, [x3, #(6 * 8)]
	ldr	x10, [x3, #(5 * 8)]
	ldr	x9, [x3, #(4 * 8)]
	ldr	x8, [x3, #(3 * 8)]
	ldr	x7, [x3, #(2 * 8)]
	ldr	x6, [x3, #(1 * 8)]
	ldr	x5, [x3, #(0 * 8)]

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26
1:
	msr	dbgbcr15_el1, x20
	msr	dbgbcr14_el1, x19
	msr	dbgbcr13_el1, x18
	msr	dbgbcr12_el1, x17
	msr	dbgbcr11_el1, x16
	msr	dbgbcr10_el1, x15
	msr	dbgbcr9_el1, x14
	msr	dbgbcr8_el1, x13
	msr	dbgbcr7_el1, x12
	msr	dbgbcr6_el1, x11
	msr	dbgbcr5_el1, x10
	msr	dbgbcr4_el1, x9
	msr	dbgbcr3_el1, x8
	msr	dbgbcr2_el1, x7
	msr	dbgbcr1_el1, x6
	msr	dbgbcr0_el1, x5

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGBVR0_EL1)

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26
1:
	ldr	x20, [x3, #(15 * 8)]
	ldr	x19, [x3, #(14 * 8)]
	ldr	x18, [x3, #(13 * 8)]
	ldr	x17, [x3, #(12 * 8)]
	ldr	x16, [x3, #(11 * 8)]
	ldr	x15, [x3, #(10 * 8)]
	ldr	x14, [x3, #(9 * 8)]
	ldr	x13, [x3, #(8 * 8)]
	ldr	x12, [x3, #(7 * 8)]
	ldr	x11, [x3, #(6 * 8)]
	ldr	x10, [x3, #(5 * 8)]
	ldr	x9, [x3, #(4 * 8)]
	ldr	x8, [x3, #(3 * 8)]
	ldr	x7, [x3, #(2 * 8)]
	ldr	x6, [x3, #(1 * 8)]
	ldr	x5, [x3, #(0 * 8)]

	adr	x26, 1f
	add	x26, x26, x24, lsl #2
	br	x26
1:
	msr	dbgbvr15_el1, x20
	msr	dbgbvr14_el1, x19
	msr	dbgbvr13_el1, x18
	msr	dbgbvr12_el1, x17
	msr	dbgbvr11_el1, x16
	msr	dbgbvr10_el1, x15
	msr	dbgbvr9_el1, x14
	msr	dbgbvr8_el1, x13
	msr	dbgbvr7_el1, x12
	msr	dbgbvr6_el1, x11
	msr	dbgbvr5_el1, x10
	msr	dbgbvr4_el1, x9
	msr	dbgbvr3_el1, x8
	msr	dbgbvr2_el1, x7
	msr	dbgbvr1_el1, x6
	msr	dbgbvr0_el1, x5

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGWCR0_EL1)

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26
1:
	ldr	x20, [x3, #(15 * 8)]
	ldr	x19, [x3, #(14 * 8)]
	ldr	x18, [x3, #(13 * 8)]
	ldr	x17, [x3, #(12 * 8)]
	ldr	x16, [x3, #(11 * 8)]
	ldr	x15, [x3, #(10 * 8)]
	ldr	x14, [x3, #(9 * 8)]
	ldr	x13, [x3, #(8 * 8)]
	ldr	x12, [x3, #(7 * 8)]
	ldr	x11, [x3, #(6 * 8)]
	ldr	x10, [x3, #(5 * 8)]
	ldr	x9, [x3, #(4 * 8)]
	ldr	x8, [x3, #(3 * 8)]
	ldr	x7, [x3, #(2 * 8)]
	ldr	x6, [x3, #(1 * 8)]
	ldr	x5, [x3, #(0 * 8)]

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26
1:
	msr	dbgwcr15_el1, x20
	msr	dbgwcr14_el1, x19
	msr	dbgwcr13_el1, x18
	msr	dbgwcr12_el1, x17
	msr	dbgwcr11_el1, x16
	msr	dbgwcr10_el1, x15
	msr	dbgwcr9_el1, x14
	msr	dbgwcr8_el1, x13
	msr	dbgwcr7_el1, x12
	msr	dbgwcr6_el1, x11
	msr	dbgwcr5_el1, x10
	msr	dbgwcr4_el1, x9
	msr	dbgwcr3_el1, x8
	msr	dbgwcr2_el1, x7
	msr	dbgwcr1_el1, x6
	msr	dbgwcr0_el1, x5

	add	x3, x2, #CPU_SYSREG_OFFSET(DBGWVR0_EL1)

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26
1:
	ldr	x20, [x3, #(15 * 8)]
	ldr	x19, [x3, #(14 * 8)]
	ldr	x18, [x3, #(13 * 8)]
	ldr	x17, [x3, #(12 * 8)]
	ldr	x16, [x3, #(11 * 8)]
	ldr	x15, [x3, #(10 * 8)]
	ldr	x14, [x3, #(9 * 8)]
	ldr	x13, [x3, #(8 * 8)]
	ldr	x12, [x3, #(7 * 8)]
	ldr	x11, [x3, #(6 * 8)]
	ldr	x10, [x3, #(5 * 8)]
	ldr	x9, [x3, #(4 * 8)]
	ldr	x8, [x3, #(3 * 8)]
	ldr	x7, [x3, #(2 * 8)]
	ldr	x6, [x3, #(1 * 8)]
	ldr	x5, [x3, #(0 * 8)]

	adr	x26, 1f
	add	x26, x26, x25, lsl #2
	br	x26
1:
	msr	dbgwvr15_el1, x20
	msr	dbgwvr14_el1, x19
	msr	dbgwvr13_el1, x18
	msr	dbgwvr12_el1, x17
	msr	dbgwvr11_el1, x16
	msr	dbgwvr10_el1, x15
	msr	dbgwvr9_el1, x14
	msr	dbgwvr8_el1, x13
	msr	dbgwvr7_el1, x12
	msr	dbgwvr6_el1, x11
	msr	dbgwvr5_el1, x10
	msr	dbgwvr4_el1, x9
	msr	dbgwvr3_el1, x8
	msr	dbgwvr2_el1, x7
	msr	dbgwvr1_el1, x6
	msr	dbgwvr0_el1, x5

	ldr	x21, [x2, #CPU_SYSREG_OFFSET(MDCCINT_EL1)]
	msr	mdccint_el1, x21
.endm

.macro skip_32bit_state tmp, target
	// Skip 32bit state if not needed
	mrs	\tmp, hcr_el2
	tbnz	\tmp, #HCR_RW_SHIFT, \target
.endm

.macro skip_tee_state tmp, target
	// Skip ThumbEE state if not needed
	mrs	\tmp, id_pfr0_el1
	tbz	\tmp, #12, \target
.endm

.macro skip_debug_state tmp, target
	ldr	\tmp, [x0, #VCPU_DEBUG_FLAGS]
	tbz	\tmp, #KVM_ARM64_DEBUG_DIRTY_SHIFT, \target
.endm

.macro compute_debug_state target
	// Compute debug state: If any of KDE, MDE or KVM_ARM64_DEBUG_DIRTY
	// is set, we do a full save/restore cycle and disable trapping.
	add	x25, x0, #VCPU_CONTEXT

	// Check the state of MDSCR_EL1
	ldr	x25, [x25, #CPU_SYSREG_OFFSET(MDSCR_EL1)]
	and	x26, x25, #DBG_MDSCR_KDE
	and	x25, x25, #DBG_MDSCR_MDE
	adds	xzr, x25, x26
	b.eq	9998f		// Nothing to see there

	// If any interesting bits was set, we must set the flag
	mov	x26, #KVM_ARM64_DEBUG_DIRTY
	str	x26, [x0, #VCPU_DEBUG_FLAGS]
	b	9999f		// Don't skip restore

9998:
	// Otherwise load the flags from memory in case we recently
	// trapped
	skip_debug_state x25, \target
9999:
.endm

.macro save_guest_32bit_state
	skip_32bit_state x3, 1f

	add	x3, x2, #CPU_SPSR_OFFSET(KVM_SPSR_ABT)
	mrs	x4, spsr_abt
	mrs	x5, spsr_und
	mrs	x6, spsr_irq
	mrs	x7, spsr_fiq
	stp	x4, x5, [x3]
	stp	x6, x7, [x3, #16]

	add	x3, x2, #CPU_SYSREG_OFFSET(DACR32_EL2)
	mrs	x4, dacr32_el2
	mrs	x5, ifsr32_el2
	mrs	x6, fpexc32_el2
	stp	x4, x5, [x3]
	str	x6, [x3, #16]

	skip_debug_state x8, 2f
	mrs	x7, dbgvcr32_el2
	str	x7, [x3, #24]
2:
	skip_tee_state x8, 1f

	add	x3, x2, #CPU_SYSREG_OFFSET(TEECR32_EL1)
	mrs	x4, teecr32_el1
	mrs	x5, teehbr32_el1
	stp	x4, x5, [x3]
1:
.endm

.macro restore_guest_32bit_state
	skip_32bit_state x3, 1f

	add	x3, x2, #CPU_SPSR_OFFSET(KVM_SPSR_ABT)
	ldp	x4, x5, [x3]
	ldp	x6, x7, [x3, #16]
	msr	spsr_abt, x4
	msr	spsr_und, x5
	msr	spsr_irq, x6
	msr	spsr_fiq, x7

	add	x3, x2, #CPU_SYSREG_OFFSET(DACR32_EL2)
	ldp	x4, x5, [x3]
	ldr	x6, [x3, #16]
	msr	dacr32_el2, x4
	msr	ifsr32_el2, x5
	msr	fpexc32_el2, x6

	skip_debug_state x8, 2f
	ldr	x7, [x3, #24]
	msr	dbgvcr32_el2, x7
2:
	skip_tee_state x8, 1f

	add	x3, x2, #CPU_SYSREG_OFFSET(TEECR32_EL1)
	ldp	x4, x5, [x3]
	msr	teecr32_el1, x4
	msr	teehbr32_el1, x5
1:
.endm

.macro activate_traps
	ldr     x2, [x0, #VCPU_HCR_EL2]
	msr     hcr_el2, x2
	mov	x2, #CPTR_EL2_TTA
	msr	cptr_el2, x2

	mov	x2, #(1 << 15)	// Trap CP15 Cr=15
	msr	hstr_el2, x2

	mrs	x2, mdcr_el2
	and	x2, x2, #MDCR_EL2_HPMN_MASK
	orr	x2, x2, #(MDCR_EL2_TPM | MDCR_EL2_TPMCR)
	orr	x2, x2, #(MDCR_EL2_TDRA | MDCR_EL2_TDOSA)

	// Check for KVM_ARM64_DEBUG_DIRTY, and set debug to trap
	// if not dirty.
	ldr	x3, [x0, #VCPU_DEBUG_FLAGS]
	tbnz	x3, #KVM_ARM64_DEBUG_DIRTY_SHIFT, 1f
	orr	x2, x2,  #MDCR_EL2_TDA
1:
	msr	mdcr_el2, x2
.endm

.macro deactivate_traps
	mov	x2, #HCR_RW
	msr	hcr_el2, x2
	msr	cptr_el2, xzr
	msr	hstr_el2, xzr

	mrs	x2, mdcr_el2
	and	x2, x2, #MDCR_EL2_HPMN_MASK
	msr	mdcr_el2, x2
.endm

.macro activate_vm
	ldr	x1, [x0, #VCPU_KVM]
	kern_hyp_va	x1
	ldr	x2, [x1, #KVM_VTTBR]
	msr	vttbr_el2, x2
.endm

.macro deactivate_vm
	msr	vttbr_el2, xzr
.endm

/*
 * Call into the vgic backend for state saving
 */
.macro save_vgic_state
	adr	x24, __vgic_sr_vectors
	ldr	x24, [x24, VGIC_SAVE_FN]
	kern_hyp_va	x24
	blr	x24
	mrs	x24, hcr_el2
	mov	x25, #HCR_INT_OVERRIDE
	neg	x25, x25
	and	x24, x24, x25
	msr	hcr_el2, x24
.endm

/*
 * Call into the vgic backend for state restoring
 */
.macro restore_vgic_state
	mrs	x24, hcr_el2
	ldr	x25, [x0, #VCPU_IRQ_LINES]
	orr	x24, x24, #HCR_INT_OVERRIDE
	orr	x24, x24, x25
	msr	hcr_el2, x24
	adr	x24, __vgic_sr_vectors
	ldr	x24, [x24, #VGIC_RESTORE_FN]
	kern_hyp_va	x24
	blr	x24
.endm

.macro save_timer_state
	// x0: vcpu pointer
	ldr	x2, [x0, #VCPU_KVM]
	kern_hyp_va x2
	ldr	w3, [x2, #KVM_TIMER_ENABLED]
	cbz	w3, 1f

	mrs	x3, cntv_ctl_el0
	and	x3, x3, #3
	str	w3, [x0, #VCPU_TIMER_CNTV_CTL]
	bic	x3, x3, #1		// Clear Enable
	msr	cntv_ctl_el0, x3

	isb

	mrs	x3, cntv_cval_el0
	str	x3, [x0, #VCPU_TIMER_CNTV_CVAL]

1:
	// Allow physical timer/counter access for the host
	mrs	x2, cnthctl_el2
	orr	x2, x2, #3
	msr	cnthctl_el2, x2

	// Clear cntvoff for the host
	msr	cntvoff_el2, xzr
.endm

.macro restore_timer_state
	// x0: vcpu pointer
	// Disallow physical timer access for the guest
	// Physical counter access is allowed
	mrs	x2, cnthctl_el2
	orr	x2, x2, #1
	bic	x2, x2, #2
	msr	cnthctl_el2, x2

	ldr	x2, [x0, #VCPU_KVM]
	kern_hyp_va x2
	ldr	w3, [x2, #KVM_TIMER_ENABLED]
	cbz	w3, 1f

	ldr	x3, [x2, #KVM_TIMER_CNTVOFF]
	msr	cntvoff_el2, x3
	ldr	x2, [x0, #VCPU_TIMER_CNTV_CVAL]
	msr	cntv_cval_el0, x2
	isb

	ldr	w2, [x0, #VCPU_TIMER_CNTV_CTL]
	and	x2, x2, #3
	msr	cntv_ctl_el0, x2
1:
.endm

__save_sysregs:
	save_sysregs
	ret

__restore_sysregs:
	restore_sysregs
	ret

__save_debug:
	save_debug
	ret

__restore_debug:
	restore_debug
	ret

__save_fpsimd:
	save_fpsimd
	ret

__restore_fpsimd:
	restore_fpsimd
	ret

/*
 * u64 __kvm_vcpu_run(struct kvm_vcpu *vcpu);
 *
 * This is the world switch. The first half of the function
 * deals with entering the guest, and anything from __kvm_vcpu_return
 * to the end of the function deals with reentering the host.
 * On the enter path, only x0 (vcpu pointer) must be preserved until
 * the last moment. On the exit path, x0 (vcpu pointer) and x1 (exception
 * code) must both be preserved until the epilogue.
 * In both cases, x2 points to the CPU context we're saving/restoring from/to.
 */
ENTRY(__kvm_vcpu_run)
	kern_hyp_va	x0
	msr	tpidr_el2, x0	// Save the vcpu register

	// Host context
	ldr	x2, [x0, #VCPU_HOST_CONTEXT]
	kern_hyp_va x2

	save_host_regs
	bl __save_fpsimd
	bl __save_sysregs

	compute_debug_state 1f
	bl	__save_debug
1:
	activate_traps
	activate_vm

	restore_vgic_state
	restore_timer_state

	// Guest context
	add	x2, x0, #VCPU_CONTEXT

	bl __restore_sysregs
	bl __restore_fpsimd

	skip_debug_state x3, 1f
	bl	__restore_debug
1:
	restore_guest_32bit_state
	restore_guest_regs

	// That's it, no more messing around.
	eret

__kvm_vcpu_return:
	// Assume x0 is the vcpu pointer, x1 the return code
	// Guest's x0-x3 are on the stack

	// Guest context
	add	x2, x0, #VCPU_CONTEXT

	save_guest_regs
	bl __save_fpsimd
	bl __save_sysregs

	skip_debug_state x3, 1f
	bl	__save_debug
1:
	save_guest_32bit_state

	save_timer_state
	save_vgic_state

	deactivate_traps
	deactivate_vm

	// Host context
	ldr	x2, [x0, #VCPU_HOST_CONTEXT]
	kern_hyp_va x2

	bl __restore_sysregs
	bl __restore_fpsimd

	skip_debug_state x3, 1f
	// Clear the dirty flag for the next run, as all the state has
	// already been saved. Note that we nuke the whole 64bit word.
	// If we ever add more flags, we'll have to be more careful...
	str	xzr, [x0, #VCPU_DEBUG_FLAGS]
	bl	__restore_debug
1:
	restore_host_regs

	mov	x0, x1
	ret
END(__kvm_vcpu_run)

// void __kvm_tlb_flush_vmid_ipa(struct kvm *kvm, phys_addr_t ipa);
ENTRY(__kvm_tlb_flush_vmid_ipa)
	dsb	ishst

	kern_hyp_va	x0
	ldr	x2, [x0, #KVM_VTTBR]
	msr	vttbr_el2, x2
	isb

	/*
	 * We could do so much better if we had the VA as well.
	 * Instead, we invalidate Stage-2 for this IPA, and the
	 * whole of Stage-1. Weep...
	 */
	tlbi	ipas2e1is, x1
	/*
	 * We have to ensure completion of the invalidation at Stage-2,
	 * since a table walk on another CPU could refill a TLB with a
	 * complete (S1 + S2) walk based on the old Stage-2 mapping if
	 * the Stage-1 invalidation happened first.
	 */
	dsb	ish
	tlbi	vmalle1is
	dsb	ish
	isb

	msr	vttbr_el2, xzr
	ret
ENDPROC(__kvm_tlb_flush_vmid_ipa)

ENTRY(__kvm_flush_vm_context)
	dsb	ishst
	tlbi	alle1is
	ic	ialluis
	dsb	ish
	ret
ENDPROC(__kvm_flush_vm_context)

	// struct vgic_sr_vectors __vgi_sr_vectors;
	.align 3
ENTRY(__vgic_sr_vectors)
	.skip	VGIC_SR_VECTOR_SZ
ENDPROC(__vgic_sr_vectors)

__kvm_hyp_panic:
	// Guess the context by looking at VTTBR:
	// If zero, then we're already a host.
	// Otherwise restore a minimal host context before panicing.
	mrs	x0, vttbr_el2
	cbz	x0, 1f

	mrs	x0, tpidr_el2

	deactivate_traps
	deactivate_vm

	ldr	x2, [x0, #VCPU_HOST_CONTEXT]
	kern_hyp_va x2

	bl __restore_sysregs

1:	adr	x0, __hyp_panic_str
	adr	x1, 2f
	ldp	x2, x3, [x1]
	sub	x0, x0, x2
	add	x0, x0, x3
	mrs	x1, spsr_el2
	mrs	x2, elr_el2
	mrs	x3, esr_el2
	mrs	x4, far_el2
	mrs	x5, hpfar_el2
	mrs	x6, par_el1
	mrs	x7, tpidr_el2

	mov	lr, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_D_BIT |\
		      PSR_MODE_EL1h)
	msr	spsr_el2, lr
	ldr	lr, =panic
	msr	elr_el2, lr
	eret

	.align	3
2:	.quad	HYP_PAGE_OFFSET
	.quad	PAGE_OFFSET
ENDPROC(__kvm_hyp_panic)

__hyp_panic_str:
	.ascii	"HYP panic:\nPS:%08x PC:%p ESR:%p\nFAR:%p HPFAR:%p PAR:%p\nVCPU:%p\n\0"

	.align	2

/*
 * u64 kvm_call_hyp(void *hypfn, ...);
 *
 * This is not really a variadic function in the classic C-way and care must
 * be taken when calling this to ensure parameters are passed in registers
 * only, since the stack will change between the caller and the callee.
 *
 * Call the function with the first argument containing a pointer to the
 * function you wish to call in Hyp mode, and subsequent arguments will be
 * passed as x0, x1, and x2 (a maximum of 3 arguments in addition to the
 * function pointer can be passed).  The function being called must be mapped
 * in Hyp mode (see init_hyp_mode in arch/arm/kvm/arm.c).  Return values are
 * passed in r0 and r1.
 *
 * A function pointer with a value of 0 has a special meaning, and is
 * used to implement __hyp_get_vectors in the same way as in
 * arch/arm64/kernel/hyp_stub.S.
 */
ENTRY(kvm_call_hyp)
	hvc	#0
	ret
ENDPROC(kvm_call_hyp)

.macro invalid_vector	label, target
	.align	2
\label:
	b \target
ENDPROC(\label)
.endm

	/* None of these should ever happen */
	invalid_vector	el2t_sync_invalid, __kvm_hyp_panic
	invalid_vector	el2t_irq_invalid, __kvm_hyp_panic
	invalid_vector	el2t_fiq_invalid, __kvm_hyp_panic
	invalid_vector	el2t_error_invalid, __kvm_hyp_panic
	invalid_vector	el2h_sync_invalid, __kvm_hyp_panic
	invalid_vector	el2h_irq_invalid, __kvm_hyp_panic
	invalid_vector	el2h_fiq_invalid, __kvm_hyp_panic
	invalid_vector	el2h_error_invalid, __kvm_hyp_panic
	invalid_vector	el1_sync_invalid, __kvm_hyp_panic
	invalid_vector	el1_irq_invalid, __kvm_hyp_panic
	invalid_vector	el1_fiq_invalid, __kvm_hyp_panic
	invalid_vector	el1_error_invalid, __kvm_hyp_panic

el1_sync:					// Guest trapped into EL2
	push	x0, x1
	push	x2, x3

	mrs	x1, esr_el2
	lsr	x2, x1, #ESR_EL2_EC_SHIFT

	cmp	x2, #ESR_EL2_EC_HVC64
	b.ne	el1_trap

	mrs	x3, vttbr_el2			// If vttbr is valid, the 64bit guest
	cbnz	x3, el1_trap			// called HVC

	/* Here, we're pretty sure the host called HVC. */
	pop	x2, x3
	pop	x0, x1

	/* Check for __hyp_get_vectors */
	cbnz	x0, 1f
	mrs	x0, vbar_el2
	b	2f

1:	push	lr, xzr

	/*
	 * Compute the function address in EL2, and shuffle the parameters.
	 */
	kern_hyp_va	x0
	mov	lr, x0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	blr	lr

	pop	lr, xzr
2:	eret

el1_trap:
	/*
	 * x1: ESR
	 * x2: ESR_EC
	 */
	cmp	x2, #ESR_EL2_EC_DABT
	mov	x0, #ESR_EL2_EC_IABT
	ccmp	x2, x0, #4, ne
	b.ne	1f		// Not an abort we care about

	/* This is an abort. Check for permission fault */
	and	x2, x1, #ESR_EL2_FSC_TYPE
	cmp	x2, #FSC_PERM
	b.ne	1f		// Not a permission fault

	/*
	 * Check for Stage-1 page table walk, which is guaranteed
	 * to give a valid HPFAR_EL2.
	 */
	tbnz	x1, #7, 1f	// S1PTW is set

	/* Preserve PAR_EL1 */
	mrs	x3, par_el1
	push	x3, xzr

	/*
	 * Permission fault, HPFAR_EL2 is invalid.
	 * Resolve the IPA the hard way using the guest VA.
	 * Stage-1 translation already validated the memory access rights.
	 * As such, we can use the EL1 translation regime, and don't have
	 * to distinguish between EL0 and EL1 access.
	 */
	mrs	x2, far_el2
	at	s1e1r, x2
	isb

	/* Read result */
	mrs	x3, par_el1
	pop	x0, xzr			// Restore PAR_EL1 from the stack
	msr	par_el1, x0
	tbnz	x3, #0, 3f		// Bail out if we failed the translation
	ubfx	x3, x3, #12, #36	// Extract IPA
	lsl	x3, x3, #4		// and present it like HPFAR
	b	2f

1:	mrs	x3, hpfar_el2
	mrs	x2, far_el2

2:	mrs	x0, tpidr_el2
	str	w1, [x0, #VCPU_ESR_EL2]
	str	x2, [x0, #VCPU_FAR_EL2]
	str	x3, [x0, #VCPU_HPFAR_EL2]

	mov	x1, #ARM_EXCEPTION_TRAP
	b	__kvm_vcpu_return

	/*
	 * Translation failed. Just return to the guest and
	 * let it fault again. Another CPU is probably playing
	 * behind our back.
	 */
3:	pop	x2, x3
	pop	x0, x1

	eret

el1_irq:
	push	x0, x1
	push	x2, x3
	mrs	x0, tpidr_el2
	mov	x1, #ARM_EXCEPTION_IRQ
	b	__kvm_vcpu_return

	.ltorg

	.align 11

ENTRY(__kvm_hyp_vector)
	ventry	el2t_sync_invalid		// Synchronous EL2t
	ventry	el2t_irq_invalid		// IRQ EL2t
	ventry	el2t_fiq_invalid		// FIQ EL2t
	ventry	el2t_error_invalid		// Error EL2t

	ventry	el2h_sync_invalid		// Synchronous EL2h
	ventry	el2h_irq_invalid		// IRQ EL2h
	ventry	el2h_fiq_invalid		// FIQ EL2h
	ventry	el2h_error_invalid		// Error EL2h

	ventry	el1_sync			// Synchronous 64-bit EL1
	ventry	el1_irq				// IRQ 64-bit EL1
	ventry	el1_fiq_invalid			// FIQ 64-bit EL1
	ventry	el1_error_invalid		// Error 64-bit EL1

	ventry	el1_sync			// Synchronous 32-bit EL1
	ventry	el1_irq				// IRQ 32-bit EL1
	ventry	el1_fiq_invalid			// FIQ 32-bit EL1
	ventry	el1_error_invalid		// Error 32-bit EL1
ENDPROC(__kvm_hyp_vector)

	.popsection