#include "config.h"
#include "vm.h"
#include "rq.h"
#include "util.h"
#include "mem.h"

void vm_rq_init(struct vm* vm, const struct vm_config* config) {
    // vaddr_t va;
    // paddr_t pa;
    vm->rq.vbase = config->rq_vm.vbase;
    vm->rq.rq_size = config->rq_vm.rq_size;
    // va = mem_alloc_map(&vm->as, NULL, vm->rq.vbase, NUM_PAGES(vm->rq.rq_size), PTE_VM_FLAGS);
    // if (va != vm->rq.vbase) {
    //     ERROR("va != vm->rq.vbase");
    // }
    // mem_translate(&vm->as, va, &pa);
    // vm->rq.pbase = pa;
}

void rq_open_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    // return
    // x0: vmid
    // x1: vbase
    // x2: rq size
    INFO("call rq_open");
    vaddr_t va;
    paddr_t pa;
    struct vm* current_vm = NULL;

    INFO("zzzzzzzzzzzzzz%x\n", CURRENT_VM->rq.vbase);
    va = mem_alloc_map(&CURRENT_VM->as, NULL, CURRENT_VM->rq.vbase, NUM_PAGES(CURRENT_VM->rq.rq_size), PTE_VM_FLAGS);
    if (va != CURRENT_VM->rq.vbase) {
        ERROR("va != vm->rq.vbase va = %x, vbase = %x", va, CURRENT_VM->rq.vbase);
    }
    mem_translate(&CURRENT_VM->as, va, &pa);
    CURRENT_VM->rq.pbase = pa;

    // INFO("111vmid = %d, vbase = %x, size = %x pa = %x", CURRENT_VM->id, CURRENT_VM->rq.vbase, CURRENT_VM->rq.rq_size, pa);
    vcpu_writereg(cpu()->vcpu, 0, CURRENT_VM->id);
    vcpu_writereg(cpu()->vcpu, 1, CURRENT_VM->rq.vbase);
    vcpu_writereg(cpu()->vcpu, 2, CURRENT_VM->rq.rq_size);
    INFO("Write vmid = %d, vbase = %x, size = %x and pa = %x", cpu()->vcpu->regs.x[0], cpu()->vcpu->regs.x[1], cpu()->vcpu->regs.x[2], pa);
    current_vm = get_vm_by_id(CURRENT_VM->id);
    if (current_vm == NULL) {
        ERROR("Can't find VM by id = %d", CURRENT_VM->id);
    }
    current_vm->rq.pbase = CURRENT_VM->rq.pbase;
    current_vm->rq.rq_size = CURRENT_VM->rq.rq_size;
    current_vm->rq.vbase = CURRENT_VM->rq.vbase;
    INFO("current vm id = %d, pbase = %x, vbase = %x, size = %x", current_vm->id, current_vm->rq.pbase, current_vm->rq.vbase, current_vm->rq.rq_size);
    INFO("Call rq_open end");
}

void rq_close_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    // true : 释放物理内存
    mem_unmap(&CURRENT_VM->as, CURRENT_VM->rq.vbase, NUM_PAGES(CURRENT_VM->rq.rq_size), true);
}

void rq_attach_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    // arg0： 接收方VMID
    // arg1： 要映射的va
    vmid_t rec_vm_id = arg0;
    vaddr_t attach_rq_va = arg1;
    struct vm *rec_vm = get_vm_by_id(rec_vm_id);
    vaddr_t va;
    paddr_t pa;
    struct ppages pages;

    INFO("Call rq_attach");
    // vaddr_t align_va = ALIGN(attach_rq_va);
    INFO("rec vm id = %d, attach rq va = %x, align(va) = %x", rec_vm_id, attach_rq_va, ALIGN(attach_rq_va, PAGE_SIZE));

    if (rec_vm == NULL) {
        ERROR("Don't find vm by %d", rec_vm_id);
    }

    // 映射recvm rq到自己的内存中
    // va 设置为自己
    pa = rec_vm->rq.pbase;
    va = attach_rq_va;
    // INFO("papapa = %x", pa);
    pages = mem_ppages_get(pa, NUM_PAGES(rec_vm->rq.rq_size));
    INFO("pages base = 0X%x", pages.base);
    mem_map(&CURRENT_VM->as, va, &pages,  NUM_PAGES(rec_vm->rq.rq_size), PTE_VM_FLAGS);
    // va = mem_alloc_map(&CURRENT_VM->as, &pages, attach_rq_va, NUM_PAGES(rec_vm->rq.rq_size), PTE_VM_FLAGS);
    // if (va != attach_rq_va) {
    //     ERROR("va != attach_rq_va");
    // }
    INFO("alloc va = %x attach va = %x, va & ~(PAGE_SIZE - 1) = %x", va, attach_rq_va, va & ~(PAGE_SIZE - 1));
    mem_translate(&CURRENT_VM->as, va, &pa);
    if (pa != rec_vm->rq.pbase) {
        ERROR("pa != rec_vm->rq.pbasem, pa = 0x%x, rec_vm->rq.pbase = 0x%x", pa, rec_vm->rq.pbase);
    } else {
        INFO("rq pa = 0x%x rec_vm->rq.pbase = 0x%x", pa, rec_vm->rq.pbase);
    }

    //返回attach rq的va 和 size
    vcpu_writereg(cpu()->vcpu, 0, va);
    vcpu_writereg(cpu()->vcpu, 1, rec_vm->rq.rq_size);
    INFO("Call rq_attach end");
}

void rq_detach_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    // 参数
    // vmm不管理attach的队列信息
    // arg0: detach rq va
    // arg1: detach rq size
    vaddr_t va = arg0;
    size_t rq_size = arg1;

    mem_unmap(&CURRENT_VM->as, va, NUM_PAGES(rq_size), false);
}