/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include "iommu.h"
#include "vm.h"
#include "cpu.h"
#include "mem.h"
#include "platform.h"
#if (SMMU_VERSION == SMMUV2)
#include "smmuv2.h"
#elif (SMMU_VERSION ==SMMUV3)
#include "smmuv3.h"
#endif

#if (SMMU_VERSION == SMMUV2)

bool iommu_arch_init() {   
    if(cpu()->id == CPU_MASTER && platform.arch.smmu.base){
        smmu_init();
        return true;
    }

    return false;
}

static size_t iommu_vm_arch_init_ctx(struct vm *vm) {
    size_t ctx_id = vm->io.mmu.ctx_id;
    if (ctx_id < 0) {

        /* Set up ctx bank to vm address space in an available ctx. */
        ctx_id = smmu_alloc_ctxbnk();
        if (ctx_id >= 0) {
            paddr_t rootpt = (paddr_t) vm->as.pt.root;
            smmu_write_ctxbnk(ctx_id, rootpt, vm->id);
            vm->io.mmu.ctx_id = ctx_id;
        } else {
            INFO("iommu: smmuv2 could not allocate ctx for vm: %d", vm->id);
        }
    }

    /* Ctx is valid when we get here. */
    return ctx_id;
}

static bool iommu_vm_arch_add(struct vm *vm, streamid_t mask, streamid_t id) {
    size_t vm_ctx = iommu_vm_arch_init_ctx(vm);
    streamid_t glbl_mask = vm->io.mmu.global_mask;
    streamid_t prep_mask = (mask & SMMU_ID_MSK) | glbl_mask;
    streamid_t prep_id = (id & SMMU_ID_MSK);
    bool group = (bool) mask;
    
    if(vm_ctx < 0){
        return false;
    }

    if (!smmu_compatible_sme_exists(prep_mask, prep_id, vm_ctx, group)) {
        size_t sme = smmu_alloc_sme();
        if(sme < 0){
            INFO("iommu: smmuv2 no more free sme available.");
            return false;
        }
        smmu_write_sme(sme, prep_mask, prep_id, group);
        smmu_write_s2c(sme, vm_ctx);
    }

    return true;
}

bool iommu_arch_vm_add_device(struct vm *vm, streamid_t id) {
    return iommu_vm_arch_add(vm, 0, id);
}

bool iommu_arch_vm_init(struct vm *vm, const struct vm_config *config) {
    vm->io.mmu.global_mask = 
        config->arch.smmu.global_mask | platform.arch.smmu.global_mask;
    vm->io.mmu.ctx_id = -1;

    /* This section relates only to arm's iommu so we parse it here. */
    for (size_t i = 0; i < config->arch.smmu.group_num; i++) {
        /* Register each group. */
        const struct smmu_group *group =
            &config->arch.smmu.groups[i];
        if(!iommu_vm_arch_add(vm, group->mask, group->id)){
            return false;
        }
    }

    return true;
}

#elif (SMMU_VERSION == SMMUV3)

bool iommu_arch_init() {   
    if(cpu()->id == CPU_MASTER && platform.arch.smmu.base){
        arm_smmuv3_init();
        return true;
    }

    return false;
}

bool iommu_arch_vm_add_device(struct vm *vm, streamid_t id) {
    return arm_smmuv3_vm_dev_init(vm->id, id) == 0;
}

bool iommu_arch_vm_init(struct vm *vm, const struct vm_config *config) {
    /* This section relates only to arm's iommu so we parse it here. */
    for (size_t i = 0; i < config->arch.smmu.group_num; i++) {
        /* Register each group. */
        const struct smmu_group *group =
            &config->arch.smmu.groups[i];
        if(arm_smmuv3_vm_dev_init(vm->id, group->id) != 0){
            return false;
        }
    }

    return true;
}

#else

bool iommu_arch_init() { 
    WARNING("NO IOMMU");  
    return true;
}

bool iommu_arch_vm_add_device(struct vm *vm, streamid_t id) {
    return true;
}

bool iommu_arch_vm_init(struct vm *vm, const struct vm_config *config) {
    return true;
}

#endif