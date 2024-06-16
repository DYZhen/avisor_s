/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include "platform.h"

struct platform platform = {
    .arch = {
        .gic = {
            .gicd_addr = 0x08000000,
            .gicc_addr = 0x08010000,
            .gich_addr = 0x08030000,
            .gicv_addr = 0x08040000,
            .gicr_addr = 0x080A0000,
            .maintenance_id = 25
        },
        .smmu = {
            .base = 0x9050000,
            .interrupt_id = 187
        },
    }

};
