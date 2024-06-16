#include "util.h"
#include "vmm.h"
#include "vm.h"
#include "string.h"
#include "config.h"
#include "sysregs.h"
#include "vmm.h"
#include "cpu.h"
#include "io.h"
#include "interrupts.h"
// #include "rq.h"

static struct vm_assignment {
    spinlock_t lock;
    bool master;
    size_t ncpus;   // VM 所拥有的VCPU的数量
    cpumap_t cpus;  // VM 所属的 VCPU 对应的位图
    struct vm_allocation vm_alloc;
} vm_assign[MAX_VM_NUM];

static bool vmm_assign_vcpu() {
    // vm_id 属性考虑弃用

    // 原设计采用 assigned 的原因是，由于 VCPU 与 PCPU 是 1:1 绑定的，而每个 PCPU 都会执行这段代码
    // 所以利用 assigned 使每个 PCPU 在运行过程中只负责与其相应的 VCPU
    
    // TODO  Finish 而在我们的设计中，需要使 master pcpu 完成全部 VM 的全部 vcpu 分配工作
    
    for (size_t i = 0; i < config.nr_vms; i++) {
        spin_lock(&vm_assign[i].lock);
        if (!vm_assign[i].master) {
            vm_assign[i].master = true;
            vm_assign[i].ncpus = config.vm[i].nr_cpus;
        }
        spin_unlock(&vm_assign[i].lock);
    }
    return true;
}

static bool vmm_alloc_vm(struct vm_allocation* vm_alloc, struct vm_config *config) {

    /**
     * We know that we will allocate a block aligned to the PAGE_SIZE, which
     * is guaranteed to fulfill the alignment of all types.
     * However, to guarantee the alignment of all fields, when we calculate 
     * the size of a field in the vm_allocation struct, we must align the
     * previous total size calculated until that point, to the alignment of 
     * the type of the next field.
     */

    size_t total_size = sizeof(struct vm);
    size_t vcpus_offset = ALIGN(total_size, _Alignof(struct vcpu));
    total_size = vcpus_offset + (config->nr_cpus * sizeof(struct vcpu));
    total_size = ALIGN(total_size, PAGE_SIZE);

    void* allocation = mem_alloc_page(NUM_PAGES(total_size), false);
    if (allocation == NULL) {
        return false;
    }
    memset((void*)allocation, 0, total_size);
    
    // 该函数实现的主要功能是为 VM 分配相应空间
    vm_alloc->base = (vaddr_t) allocation;
    vm_alloc->size = total_size;
    vm_alloc->vm = (struct vm*) vm_alloc->base;
    vm_alloc->vcpus = (struct vcpu*) (vm_alloc->base + vcpus_offset);

    return true;
}

static struct vm_allocation* vmm_alloc_install_vm(vmid_t vm_id) {
    struct vm_allocation *vm_alloc = &vm_assign[vm_id].vm_alloc;
    if (!vmm_alloc_vm(vm_alloc, &config.vm[vm_id])) {
        ERROR("Failed to allocate vm internal structures");
    }
    fence_ord_write();

    return vm_alloc;
}

void vmm_io_init() {
    io_init();
}

void vm_list_init() {
    INIT_LIST_HEAD(&vm_list.list);
}

void vmm_init() {
    vmm_arch_init();
    
    vmm_io_init();  // AVISOR WARNING: NO IOMMU
    // ipc_init()

    cpu_sync_barrier(&cpu_glb_sync);

    vm_list_init();

    if (cpu()->id == 0) {
        vmm_assign_vcpu();
        // 为 VM 分配相应空间，同样使 master pcpu 完成全部 VM 的空间分配工作
        for (int i = 0; i < config.nr_vms; ++i) {
            INFO("VMID:%d Load addr: 0x%x", i, config.vm[i].load_addr);
            struct vm_allocation *vm_alloc = vmm_alloc_install_vm(i);
            struct vm_config *vm_config = &config.vm[i];
            // 完成 VM 的初始化工作
            struct vm *vm = vm_init(vm_alloc, vm_config, i);
        }
    }
    
    cpu_sync_barrier(&cpu_glb_sync);
//    transfer_rq();
    try_reschedule();
    // rr
//    rr_try_reschedule(1, 1);
    // TODO FINISH 该函数不会返回，应该是已经运行 vcpu 上的任务了
    vcpu_run(cpu()->vcpu);
}
