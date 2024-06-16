#ifndef VM_H
#define VM_H

#include "util.h"
#include "arch_vm.h"
#include "mem.h"
#include "cpu.h"
#include "io.h"
#include "arch_interrupts.h"
#include "list.h"
#include "emul.h"
#include "config.h"
#include "rq.h"

struct vm_mem_region {
    paddr_t base;
    size_t size;
    struct page_pool page_pool;
};

struct vm_dev_region {
    devid_t id;
    paddr_t pa;
    vaddr_t va;
    size_t size;
    // irqid_t iqr_id;
    size_t interrupt_num;
    irqid_t *interrupts;
};

struct vm;

struct vcpu {
    vcpuid_t id;
    cpuid_t p_id;
    
    struct vcpu_regs regs;
    struct vcpu_arch arch;
    
    struct vm *vm;

//    uint8_t stack[STACK_SIZE] __attribute__((aligned(PAGE_SIZE)));
//} __attribute__((aligned(PAGE_SIZE)));
};

struct vm_platform {
    size_t cpu_num;
    struct arch_vm_platform arch;
};

struct vm {
    vmid_t id;

    const struct vm_config* vm_config;

    struct vcpu* vcpus; // vm 所分配的全部 vcpu
    size_t nr_cpus;
    cpumap_t cpus;      // TODO 用于标记
    cpuid_t master;     // TODO FINISH 初始化完成后，存储 BFS 分配的 PCPU id
    
    struct cpu_synctoken sync;
    spinlock_t lock;

    struct addr_space as;

    struct vm_arch arch;

    struct list_head emul_mem_list;
    struct list_head emul_reg_list;
    struct list_head list;  //vm list 

    struct rq_vm rq;

    struct vm_io io;
    
    // MODIFY
//    bmqMultiQueue* bmqueue;
//    vCpuNode* run_vnode;

    BITMAP_ALLOC(interrupt_bitmap, MAX_INTERRUPTS);
};

// 理论上，该结构体中的 vm 与 vcpus 只是获取了相应的空间，空间内的数据并没有被初始化
struct vm_allocation {
    vaddr_t base;
    size_t size;
    struct vm *vm;
    struct vcpu *vcpus;
};

struct vm_list {
    spinlock_t lock;
    struct list_head list;
};
extern struct vm_list vm_list;

void vcpu_arch_run(struct vcpu* vcpu);

struct vm* vm_init(struct vm_allocation* vm_alloc, const struct vm_config* vm_config, vmid_t vm_id);
void vcpu_run(struct vcpu* vcpu);
void cpu_idle();
void vm_msg_broadcast(struct vm* vm, struct cpu_msg* msg);
cpumap_t vm_translate_to_pcpu_mask(struct vm* vm, cpumap_t mask, size_t len);
cpumap_t vm_translate_to_vcpu_mask(struct vm* vm, cpumap_t mask, size_t len);
void vm_emul_add_mem(struct vm* vm, struct emul_mem* emu);
void vm_emul_add_reg(struct vm* vm, struct emul_reg* emu);
emul_handler_t vm_emul_get_mem(struct vm* vm, vaddr_t addr);
emul_handler_t vm_emul_get_reg(struct vm* vm, vaddr_t addr);

static inline struct vcpu* vm_get_vcpu(struct vm* vm, vcpuid_t vcpuid) {
    if (vcpuid < vm->nr_cpus) {
        return &vm->vcpus[vcpuid];
    }
    return NULL;
}

static inline cpuid_t vm_translate_to_pcpuid(struct vm* vm, vcpuid_t vcpuid) {
    struct vcpu *vcpu = vm_get_vcpu(vm, vcpuid);
    
    if (vcpu == NULL) {
        return INVALID_CPUID;
    } else {
        return vcpu->p_id;
    }
}


static inline vcpuid_t vm_translate_to_vcpuid(struct vm* vm, cpuid_t pcpuid) {
    if (vm->cpus & (1UL << pcpuid)) {
        return (cpuid_t)bit_count(vm->cpus & BIT_MASK(0, pcpuid));
    } else {
        return INVALID_CPUID;
    }
}

static inline bool vm_has_interrupt(struct vm* vm, irqid_t int_id) {
    return !!bitmap_get(vm->interrupt_bitmap, int_id);
}

static inline void vcpu_inject_hw_irq(struct vcpu *vcpu, irqid_t id) {
    vcpu_arch_inject_hw_irq(vcpu, id);
}

static inline void vcpu_inject_irq(struct vcpu *vcpu, irqid_t id) {
    vcpu_arch_inject_irq(vcpu, id);
}

struct vm* get_vm_by_id(vmid_t id);

void vm_arch_init(struct vm* vm, const struct vm_config* config);
void vcpu_arch_init(struct vcpu* vcpu, struct vm* vm);
void vcpu_arch_reset(struct vcpu* vcpu, vaddr_t entry);

void vcpu_writepc(struct vcpu* vcpu, size_t pc);
SREG64 vcpu_readpc(struct vcpu* vcpu);
void vcpu_writereg(struct vcpu* vcpu, size_t reg, size_t val);
SREG64 vcpu_readreg(struct vcpu* vcpu, size_t reg);

#define CURRENT_VM ((struct vm*) cpu()->vcpu->vm)

#endif