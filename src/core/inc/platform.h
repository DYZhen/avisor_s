/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "util.h"
#include "arch_platform.h"

struct platform {
    size_t cpu_num;
    bool    cpu_master_fixed;
    cpuid_t cpu_master;

    size_t region_num;
    struct mem_region *regions;

    struct {
        paddr_t base;
    } console;

    // struct cache cache;

    struct arch_platform arch;
};

extern struct platform platform;

#endif /* __PLATFORM_H__ */