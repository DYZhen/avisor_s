#include "arch_vm.h"
#include "vm.h"
#include "string.h"
#include "sysregs.h"
#include "platform.h"

SREG64 vcpu_readreg(struct vcpu* vcpu, unsigned long reg) {
    if (reg > 30) {
        return 0;
    }
    return vcpu->regs.x[reg];
}

void vcpu_writereg(struct vcpu* vcpu, unsigned long reg, unsigned long val) {
    if (reg > 30) {
        return;
    }
    vcpu->regs.x[reg] = val;
}

SREG64 vcpu_readpc(struct vcpu* vcpu) {
    return vcpu->regs.elr_el2;
}

void vcpu_writepc(struct vcpu* vcpu, unsigned long pc) {
    vcpu->regs.elr_el2 = pc;
}

void vcpu_arch_profile_init(struct vcpu* vcpu, struct vm* vm) {
    sysreg_vttbr_el2_write((((uint64_t)vm->id << VTTBR_VMID_OFF) & VTTBR_VMID_MSK) |
                       ((paddr_t)vm->as.pt.root & ~VTTBR_VMID_MSK));
    ISB();
}

static unsigned long vm_cpuid_to_mpidr(struct vm* vm, vcpuid_t cpuid) {
    unsigned long mpidr;

    if (cpuid > vm->nr_cpus) {
        return ~(~MPIDR_RES1 & MPIDR_RES0_MSK); //return an invlid mpidr by inverting res bits
    }

    mpidr = cpuid | MPIDR_RES1;

    if (vm->nr_cpus == 1) {
        mpidr |= MPIDR_U_BIT;
    }

    return mpidr;
}

void vcpu_arch_init(struct vcpu* vcpu, struct vm* vm) {
    vcpu->arch.vmpidr = vm_cpuid_to_mpidr(vm, vcpu->id);
    // TODO FINISH 将该函数放到 bmq 完成后调用
//    sysreg_vmpidr_el2_write(vcpu->arch.vmpidr);

    // vcpu->arch.psci_ctx.state = vcpu->id == 0 ? ON : OFF;

    // TODO FINISH 将此处调用注释，清除 assigned 思路，将其放到 bfs 完成后调用
    //  在调度过程中，由于 bfs 先于 bmq 调用，prepare_to_switch(struct vm* vm) 函数会完成相应工作
//    vcpu_arch_profile_init(vcpu, vm);

    vgic_cpu_init(vcpu);
}

void vcpu_arch_reset(struct vcpu* vcpu, vaddr_t entry) {
    memset(&vcpu->regs, 0, sizeof(struct vcpu_regs));

    vcpu->regs.spsr_el2 = SPSR_EL1h | SPSR_F | SPSR_I | SPSR_A | SPSR_D;

    vcpu_writepc(vcpu, entry);

    sysreg_cntvoff_el2_write(0);

    /**
     *  See ARMv8-A ARM section D1.9.1 for registers that must be in a known
     * state at reset.
     */
    sysreg_sctlr_el1_write(SCTLR_RES1);
    sysreg_cntkctl_el1_write(0);
    sysreg_pmcr_el0_write(0);

    /**
     *  TODO: ARMv8-A ARM mentions another implementation optional registers
     * that reset to a known value.
     */
}


void vcpu_arch_run(struct vcpu* vcpu) {
    vm_entry();
}

struct vcpu* vm_get_vcpu_by_mpidr(struct vm* vm, unsigned long mpidr) {
    for (cpuid_t vcpuid = 0; vcpuid < vm->nr_cpus; vcpuid++)
    {
        struct vcpu *vcpu = vm_get_vcpu(vm, vcpuid);
        if ((vcpu->arch.vmpidr & MPIDR_AFF_MSK) == (mpidr & MPIDR_AFF_MSK))  {
            return vcpu;
        }
    }

    return NULL;
}

void vm_arch_init(struct vm* vm, const struct vm_config* config) {
    if (vm->master == cpu()->id) {
        vgic_init(vm, &config->arch.gic);
    }

    cpu_sync_and_clear_msgs(&vm->sync);
}
