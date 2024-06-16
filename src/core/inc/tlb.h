#ifndef TLB_H
#define TLB_H

#include "arch_tlb.h"
#include "mmu.h"

static inline void tlb_inv_va(struct addr_space *as, vaddr_t va)
{
    if (as->type == AS_HYP) {
        tlb_hyp_inv_va(va);
    } else if (as->type == AS_VM) {
        tlb_vm_inv_va(as->asid, va);
        // TODO: inval iommu tlbs
    }
}

static inline void tlb_inv_all(struct addr_space *as)
{
    if (as->type == AS_HYP) {
        tlb_hyp_inv_all();
    } else if (as->type == AS_VM) {
        tlb_vm_inv_all(as->asid);
        // TODO: inval iommu tlbs
    }
}

#endif