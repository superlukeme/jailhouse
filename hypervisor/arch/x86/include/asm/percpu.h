/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PERCPU_H
#define _JAILHOUSE_ASM_PERCPU_H

#include <jailhouse/types.h>
#include <asm/paging.h>
#include <asm/processor.h>

#include <jailhouse/hypercall.h>

#define NUM_ENTRY_REGS			6

#define STACK_SIZE			PAGE_SIZE

#ifndef __ASSEMBLY__

#include <jailhouse/cell.h>
#include <asm/spinlock.h>
#include <asm/svm.h>
#include <asm/vmx.h>

/**
 * @defgroup Per-CPU Per-CPU Subsystem
 *
 * The per-CPU subsystem provides a CPU-local state structure and accessors.
 *
 * @{
 */

/** Per-CPU states accessible across all CPUs. */
struct public_per_cpu {
	/** Logical CPU ID (same as Linux). */
	unsigned int cpu_id;
	/** Physical APIC ID. */
	u32 apic_id;
	/** Owning cell. */
	struct cell *cell;

	/** Statistic counters. */
	u32 stats[JAILHOUSE_NUM_CPU_STATS];

	/**
	 * Lock protecting CPU state changes done for control tasks.
	 *
	 * The lock protects the following fields (unless CPU is suspended):
	 * @li public_per_cpu::suspend_cpu
	 * @li public_per_cpu::cpu_suspended (except for spinning on it to
	 *                                    become true)
	 * @li public_per_cpu::wait_for_sipi
	 * @li public_per_cpu::init_signaled
	 * @li public_per_cpu::sipi_vector
	 * @li public_per_cpu::flush_vcpu_caches
	 */
	spinlock_t control_lock;

	/** Set to true for instructing the CPU to suspend. */
	volatile bool suspend_cpu;
	/** True if CPU is waiting for SIPI. */
	volatile bool wait_for_sipi;
	/** True if CPU is suspended. */
	volatile bool cpu_suspended;
	/** Set to true for pending an INIT signal. */
	bool init_signaled;
	/** Pending SIPI vector; -1 if none is pending. */
	int sipi_vector;
	/** Set to true for a pending TLB flush for the paging layer that does
	 *  host physical <-> guest physical memory mappings. */
	bool flush_vcpu_caches;
	/** Set to true for pending cache allocation updates (Intel only). */
	bool update_cat;
	/** State of the shutdown process. Possible values:
	 * @li SHUTDOWN_NONE: no shutdown in progress
	 * @li SHUTDOWN_STARTED: shutdown in progress
	 * @li negative error code: shutdown failed
	 */
	int shutdown_state;
	/** True if CPU violated a cell boundary or cause some other failure in
	 * guest mode. */
	bool failed;
} __attribute__((aligned(PAGE_SIZE)));

/** Per-CPU states. */
struct per_cpu {
	union {
		/** Stack used while in hypervisor mode. */
		u8 stack[STACK_SIZE];
		struct {
			u8 __fill[STACK_SIZE - sizeof(union registers)];
			/** Guest registers saved on stack during VM exit. */
			union registers guest_regs;
		};
	};

	union {
		struct {
			/** VMXON region, required by VMX. */
			struct vmcs vmxon_region
				__attribute__((aligned(PAGE_SIZE)));
			/** VMCS of this CPU, required by VMX. */
			struct vmcs vmcs
				__attribute__((aligned(PAGE_SIZE)));
		};
		struct {
			/** VMCB block, required by SVM. */
			struct vmcb vmcb
				__attribute__((aligned(PAGE_SIZE)));
			/** SVM Host save area; opaque to us. */
			u8 host_state[PAGE_SIZE]
				__attribute__((aligned(PAGE_SIZE)));
		};
	};

	/** Linux stack pointer, used for handover to hypervisor. */
	unsigned long linux_sp;

	/** Linux states, used for handover to/from hypervisor. @{ */
	struct desc_table_reg linux_gdtr;
	struct desc_table_reg linux_idtr;
	unsigned long linux_reg[NUM_ENTRY_REGS];
	unsigned long linux_ip;
	unsigned long linux_cr0;
	unsigned long linux_cr3;
	unsigned long linux_cr4;
	struct segment linux_cs;
	struct segment linux_ds;
	struct segment linux_es;
	struct segment linux_fs;
	struct segment linux_gs;
	struct segment linux_tss;
	unsigned long linux_efer;
	/** @} */

	/** Shadow states. @{ */
	unsigned long pat;
	unsigned long mtrr_def_type;
	/** @} */

	/** Cached PDPTEs, used by VMX for PAE guest paging mode. */
	unsigned long pdpte[4];

	/** Per-CPU paging structures. */
	struct paging_structures pg_structs;

	volatile u32 vtd_iq_completed;

	/** True when CPU is initialized by hypervisor. */
	bool initialized;
	union {
		/** VMX initialization state */
		enum vmx_state vmx_state;
		/** SVM initialization state */
		enum {SVMOFF = 0, SVMON} svm_state;
	};

	/** Number of iterations to clear pending APIC IRQs. */
	unsigned int num_clear_apic_irqs;

	struct public_per_cpu public;
} __attribute__((aligned(PAGE_SIZE)));

/**
 * Retrieve the data structure of the current CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct per_cpu *this_cpu_data(void)
{
	return (struct per_cpu *)LOCAL_CPU_BASE;
}

/**
 * Retrieve the ID of the current CPU.
 *
 * @return CPU ID.
 */
static inline unsigned int this_cpu_id(void)
{
	return this_cpu_data()->public.cpu_id;
}

/**
 * Retrieve the cell owning the current CPU.
 *
 * @return Pointer to cell.
 */
static inline struct cell *this_cell(void)
{
	return this_cpu_data()->public.cell;
}

/**
 * Retrieve the locally accessible data structure of the specified CPU.
 * @param cpu	ID of the target CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	extern u8 __page_pool[];

	return (struct per_cpu *)(__page_pool + cpu * sizeof(struct per_cpu));
}

/**
 * Retrieve the publicly accessible data structure of the specified CPU.
 * @param cpu	ID of the target CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct public_per_cpu *public_per_cpu(unsigned int cpu)
{
	return &per_cpu(cpu)->public;
}

/** @} **/

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PERCPU_H */
