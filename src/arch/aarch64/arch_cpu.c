#include "util.h"
#include "sysregs.h"
#include "cpu.h"
#include "platform.h"

void cpu_arch_init(cpuid_t cpuid) {   
    cpu()->arch.mpidr = sysreg_mpidr_el1_read();
}

unsigned long cpu_id_to_mpidr(cpuid_t id) {
    return platform_arch_cpuid_to_mpidr(&platform, id);
}