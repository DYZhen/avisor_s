#ifndef ARCH_VM_H
#define ARCH_VM_H

#include "util.h"
#include "smmuv2.h"
#include "list.h"
#include "vgic.h"

struct vcpu_regs {
    REG64 x[31];
    SREG64 spsr_el2;
    SREG64 elr_el2;
    REG64 sp_el1;
};

struct vm_arch {
    struct vgicd vgicd;
    vaddr_t vgicr_addr;
    struct list_head vgic_spilled;
    spinlock_t vgic_spilled_lock;
    struct emul_mem vgicd_emul;
    struct emul_mem vgicr_emul;
    struct emul_reg icc_sgir_emul;
    struct emul_reg icc_sre_emul;
};

struct vcpu_arch {
    size_t vmpidr;
    struct vgic_priv vgic_priv;
    struct list_head vgic_spilled;
    // struct psci_ctx psci_ctx;
};

struct arch_vm_platform {
    struct vgic_dscrp {
        paddr_t gicd_addr;
        paddr_t gicc_addr;
        paddr_t gicr_addr;
        size_t interrupt_num;
    } gic;

    struct {
        streamid_t global_mask;
        size_t group_num;
        struct smmu_group {
            streamid_t mask;
            streamid_t id;
        } *groups;
    } smmu;
};

void vm_entry();

static inline void vcpu_arch_inject_hw_irq(struct vcpu* vcpu, irqid_t id) {
    vgic_inject_hw(vcpu, id);
}

static inline void vcpu_arch_inject_irq(struct vcpu* vcpu, irqid_t id) {
    vgic_inject(vcpu, id, 0);
}


struct vcpu* vm_get_vcpu_by_mpidr(struct vm* vm, unsigned long mpidr);

#endif