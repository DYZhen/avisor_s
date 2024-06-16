#include "arch_platform.h"
#include "sysregs.h"
#include "platform.h"

struct platform;

//需要增加多集群支持
unsigned long platform_arch_cpuid_to_mpidr(const struct platform* plat,
                                      cpuid_t cpuid) {
    if (cpuid > plat->cpu_num) {
        return ~(~MPIDR_RES1 & MPIDR_RES0_MSK); //return an invlid mpidr by inverting res bits
    }

    unsigned long mpidr = 0;
    /**
     * No cluster information in configuration. Assume a singl cluster.
     * 确实只有一个集群
    */
    mpidr = cpuid;

    mpidr |= MPIDR_RES1;
    if (plat->cpu_num == 1) {
        mpidr |= MPIDR_U_BIT;
    }

    return mpidr;
}