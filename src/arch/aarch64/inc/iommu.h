/**
 * SPDX-License-Identifier: Apache-2.0 
 * Copyright (c) Bao Project and Contributors. All rights reserved
 */

#ifndef IOMMU_H
#define IOMMU_H

#include "util.h"

#define SMMUV2 (2)
#define SMMUV3 (3)

struct vm_config;
struct vm;

struct iommu_vm_arch {
    streamid_t global_mask;
    size_t ctx_id;
};

/* Must be implemented by architecture. */
bool iommu_arch_init();
bool iommu_arch_vm_init(struct vm *vm, const struct vm_config *config);
bool iommu_arch_vm_add_device(struct vm *vm, deviceid_t id);

#endif
