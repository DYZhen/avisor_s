#include "mem_cfg.h"

.global start
start:
    adr x5, .                                   // save entry addr
    mrs x0, mpidr_el1
    and x0, x0, #0x3

    cmp x0, #0
    bne 1f

    .equ PSCI_0_2_FN64_CPU_ON, 0xc4000003       // boot other cpus
    mov x4, 1         
2:
    ldr x0, =PSCI_0_2_FN64_CPU_ON
    mov x1, x4
    mov x2, x5   
    ldr x3, =0

    smc 0

    add x4, x4, #1
    cmp x4, #8                           // max nr_cpu: 8
    bne 2b
1:
    mrs x0, mpidr_el1
    and x0, x0, #0x3
    mov x1, #CPU_BASE
    mov x2, #CPU_SIZE
    mul x3, x0, x2
    add x3, x1, x3      
    
    msr tpidr_el2, x3                   // tpidr_el2 = cpu_base + cpu_size * cpu_id
    
    mrs x0, tpidr_el2
    ldr x1, =(STACK_OFF + STACK_SIZE)
    add x0, x0, x1
    mov sp, x0                     

    ldr	x0, =vector
    msr	vbar_el2, x0                    // set vector table of el2

    // msr DAIFClr, #0x3                   // Enable Interrupts

    mrs x0, mpidr_el1                   // set args to cpuid
    and x0, x0, #0x3
    
    bl main                             // jump to c program