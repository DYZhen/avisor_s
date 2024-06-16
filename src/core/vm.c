#include "interrupts.h"
#include "vm.h"
#include "config.h"
#include "sysregs.h"
#include "fences.h"
#include "mem.h"
#include "string.h"
#include "cpu.h"
#include "list.h"
#include "sched.h"
// #include "rq.h"

struct vm_list vm_list;

static void vm_init_mem_regions(struct vm* vm, const struct vm_config* vm_config) { 
    vaddr_t va;
    paddr_t pa;

    va = mem_alloc_map(&vm->as, NULL, vm_config->base_addr, NUM_PAGES(vm_config->size + vm_config->dmem_size), PTE_VM_FLAGS);
    if (va != vm_config->base_addr) {
        ERROR("va != vm's base_addr");
    }
    mem_translate(&vm->as, va, &pa);
    memcpy((void*)pa, (void*)vm_config->load_addr, vm_config->size);
    INFO("Copy vm%d to 0x%x, size = 0x%x", vm->id, pa, vm_config->size);
        
    va = mem_alloc_map(&vm->as, NULL,
                (vaddr_t)config.dtb.base_addr, NUM_PAGES(config.dtb.size), PTE_VM_FLAGS);
    if (va != config.dtb.base_addr) {
        ERROR("va != config->vm.base_addr");
    }
    mem_translate(&vm->as, va, &pa);
    memcpy((void*)pa, (void*)config.dtb.load_addr, config.dtb.size);
    INFO("Copy dtb to 0x%x, size = 0x%x", pa, config.dtb.size);
}

static struct vm* vm_allocation_init(struct vm_allocation* vm_alloc) {
    struct vm *vm = vm_alloc->vm;
    vm->vcpus = vm_alloc->vcpus;
    return vm;
}

void vm_cpu_init(struct vm* vm) {
    spin_lock(&vm->lock);
    vm->cpus |= (1UL << cpu()->id);
    spin_unlock(&vm->lock);
}

static vcpuid_t vm_calc_vcpu_id(struct vm* vm) {
    vcpuid_t vcpu_id = 0;
    for(size_t i = 0; i < cpu()->id; i++) {
        // BAO 实现的是物理CPU与VCPU的 1:1 映射，所以这里据此获取 vcpu 的 id
        if (!!bit_get(vm->cpus, i)) vcpu_id++;
    }
    return vcpu_id;
}

void vm_vcpu_init(struct vm* vm, const struct vm_config* vm_config) {
    // 使 master pcpu 完成全部 VM 的全部 vcpu 的初始化工作
    for (int i = 0; i < vm->nr_cpus; ++i) {
//        ASSERT(i < 1);
        struct vcpu* vcpu = vm_get_vcpu(vm, i);
        vcpu->id = i;
        vcpu->vm = vm;
        // TODO FINISH 为当前 pcpu 指定了要运行的 vcpu，这应该是 bmq_scheduler 的任务
//        vcpu->p_id = cpu()->id;
//        cpu()->vcpu = vcpu;
        vcpu_arch_init(vcpu, vm);
        if (i == 0) {
            vcpu_arch_reset(vcpu, vm_config->entry);
        } else {
            vcpu_arch_reset(vcpu, 0);
        }
        
    }
}

static void vm_master_init(struct vm* vm, const struct vm_config* vm_config, vmid_t vm_id) {
    vm->master = cpu()->id; // TODO FINISH 此处逻辑先行保留，但是当初始化完成之后，调用 BFS 算法后，这里的值应该替换为相应 PCPU 的 id
    vm->nr_cpus = vm_config->nr_cpus;
    vm->id = vm_id;
    vm->vm_config = vm_config;

    cpu_sync_init(&vm->sync, vm->nr_cpus);

    as_init(&vm->as, AS_VM, vm_id, NULL);

    INIT_LIST_HEAD(&vm->emul_mem_list);
    INIT_LIST_HEAD(&vm->emul_reg_list);
}

static void vm_init_dev(struct vm* vm, const struct vm_config* config) {
    for (size_t i = 0; i < config->nr_devs; i++) {
        struct vm_dev_region* dev = &config->devs[i];

        size_t n = ALIGN(dev->size, PAGE_SIZE) / PAGE_SIZE;

        if (dev->va != INVALID_VA) {
            mem_alloc_map_dev(&vm->as, (vaddr_t)dev->va, dev->pa, n);
        }

        for (size_t j = 0; j < dev->interrupt_num; j++) {
            interrupts_vm_assign(vm, dev->interrupts[j]);
        }
    }

    if (io_vm_init(vm, config)) {
        for (size_t i = 0; i < config->nr_devs; i++) {
            struct vm_dev_region* dev = &config->devs[i];
            if (dev->id) {
                if (!io_vm_add_device(vm, dev->id)){
                    ERROR("Failed to add device to iommu");
                }
            }
        }
    }
      
}

struct vm* vm_init(struct vm_allocation* vm_alloc, const struct vm_config* vm_config, vmid_t vm_id) {
    // TODO FINISH 去除函数中 if (master) 判断，因为只有 master 会调用这里面的函数
    struct vm *vm = vm_allocation_init(vm_alloc);
    
    vm_master_init(vm, vm_config, vm_id);
    
    // TODO FINISH 从 vm_vcpu_init->vcpu_arch_init 函数中分离的逻辑
    //  尽量保证该函数在 vm_vcpu_init->vcpu_arch_init->vgic_cpu_init 之前调用
    sysreg_vttbr_el2_write((((uint64_t)vm->id << VTTBR_VMID_OFF) & VTTBR_VMID_MSK) |
                           ((paddr_t)vm->as.pt.root & ~VTTBR_VMID_MSK));
    ISB();
    
    vm_vcpu_init(vm, vm_config);
    
    // TODO FINISH 若当前 CPU 是 MASTER，执行 vgic_init(vm, &config->arch.gic);
    //  替换为等价调用
//    vm_arch_init(vm, vm_config);
    vgic_init(vm, &vm_config->arch.gic);
    
    vm_init_mem_regions(vm, vm_config);
    vm_init_dev(vm, vm_config);
    // init address space first
    vm_rq_init(vm, vm_config);
    
    spin_lock(&rq_spin_lock);
    if (rq_ptr == NULL) {
        init_pr();
        rq_ptr = initialize_rqnode(NULL, 0);
    }
    spin_unlock(&rq_spin_lock);
    
    // 构造相应的 runqueue node
    runQueueNode* rqnode = initialize_rqnode(vm, bfs_default_prio);
    
    // 构造并插入 VM 所拥有的 vcpu0
    v_task_insert(initialize_vCpuNode(&(vm->vcpus[0]), bmq_default_prio), rqnode);
    
    // rr
//    rr_vnode_insert(initialize_vCpuNode(&(vm->vcpus[0]), bmq_default_prio), rqnode);
    
    // 插入相应的 runqueue node
    rq_insert(rqnode);
    // rr
//    rr_rq_insert(rqnode);

    INIT_LIST_HEAD(&vm->list);
    spin_lock(&vm_list.lock);
    list_add(&vm->list, &vm_list.list);
    spin_unlock(&vm_list.lock);

    return vm;
}

void vcpu_run(struct vcpu* vcpu) {
    vcpu_arch_run(vcpu);
}

void vm_msg_broadcast(struct vm* vm, struct cpu_msg* msg) {
    cpu_send_msg(vm->master, msg);
//    for (size_t i = 0, n = 0; n < vm->nr_cpus - 1; i++) {
//        if (((1U << i) & vm->cpus) && (i != cpu()->id)) {
//            n++;
//            cpu_send_msg(i, msg);
//        }
//    }
}

__attribute__((weak)) cpumap_t vm_translate_to_pcpu_mask(struct vm* vm,
                                                         cpumap_t mask,
                                                         size_t len) {
    cpumap_t pmask = 0;
    cpuid_t shift;
    for (size_t i = 0; i < len; i++) {
        if ((mask & (1ULL << i)) &&
            ((shift = vm_translate_to_pcpuid(vm, i)) != INVALID_CPUID)) {
            pmask |= (1ULL << shift);
        }
    }
    return pmask;
}

__attribute__((weak)) cpumap_t vm_translate_to_vcpu_mask(struct vm* vm,
                                                         cpumap_t mask,
                                                         size_t len) {
    cpumap_t pmask = 0;
    vcpuid_t shift;
    for (size_t i = 0; i < len; i++) {
        if ((mask & (1ULL << i)) &&
            ((shift = vm_translate_to_vcpuid(vm, i)) != INVALID_CPUID)) {
            pmask |= (1ULL << shift);
        }
    }
    return pmask;
}

void vm_emul_add_mem(struct vm* vm, struct emul_mem* emu) {
    list_add_tail(&emu->list, &vm->emul_mem_list);
}

void vm_emul_add_reg(struct vm* vm, struct emul_reg* emu) {
    list_add_tail(&emu->list, &vm->emul_reg_list);
}    

emul_handler_t vm_emul_get_mem(struct vm* vm, vaddr_t addr) {
    struct emul_mem* emu = NULL;
    emul_handler_t handler = NULL;

    list_for_each_entry(emu, &vm->emul_mem_list, list) {
        if (addr >= emu->va_base && (addr < (emu->va_base + emu->size))) {
            handler = emu->handler;
            break;
        }
    }
    return handler;
}

emul_handler_t vm_emul_get_reg(struct vm* vm, vaddr_t addr) {
    struct emul_reg* emu = NULL;
    emul_handler_t handler = NULL;

    // list_foreach(vm->emul_reg_list, struct emul_reg, emu) {
    //     if(emu->addr == addr) {
    //         handler = emu->handler;
    //         break; 
    //     }
    // }
    list_for_each_entry(emu, &vm->emul_reg_list, list) {
        if (emu->addr == addr) {
            handler = emu->handler;
            break;
        }
    }

    return handler;
}

struct vm* get_vm_by_id(vmid_t id) {
    struct vm* res_vm = NULL;

    list_for_each_entry(res_vm, &vm_list.list, list) {
        if (res_vm->id == id) {
            return res_vm;
        }
    }
    return NULL;
}