#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include "util.h"
#include "sysregs.h"

struct cpu_arch {
    SREG64 mpidr;
};

static inline struct cpu* cpu() {
    size_t base = sysreg_tpidr_el2_read();
    return (struct cpu*) base;
}

unsigned long cpu_id_to_mpidr(cpuid_t id);

#endif