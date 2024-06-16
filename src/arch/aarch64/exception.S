#include "mem_cfg.h"
.section "vectors"

.global VM_EXIT
.macro VM_EXIT
	// sp = &cpu_reg
	stp	x0, x1,   [sp, #0]
	stp	x2, x3,   [sp, #16]
	stp	x4, x5,   [sp, #32]
	stp	x6, x7,   [sp, #48]
	stp	x8, x9,   [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x16, x17, [sp, #128]
	stp	x18, x19, [sp, #144]
	stp	x20, x21, [sp, #160]
	stp	x22, x23, [sp, #176]
	stp	x24, x25, [sp, #192]
	stp	x26, x27, [sp, #208]
	stp	x28, x29, [sp, #224]
	str x30,      [sp, #240]

	mrs x0, spsr_el2
	str x0, 	  [sp, #248]

//	mrs x0, elr_el2
//    stp x0, x1,   [sp, #256]
    mrs x0, elr_el2
    str x0, 	  [sp, #256]
    mrs x0, sp_el1
    str x0, 	  [sp, #264]
	
	mrs x0, tpidr_el2
    ldr x1, =(CPU_STACK_OFF + CPU_STACK_SIZE)
    add x0, x0, x1
    mov sp, x0
	
.endm


.macro SAVE_CPU
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x8, x9, [sp, #-16]!
	stp	x10, x11, [sp, #-16]!
	stp	x12, x13, [sp, #-16]!
	stp	x14, x15, [sp, #-16]!
	stp	x16, x16, [sp, #-16]!
	stp	x18, x19, [sp, #-16]!
	stp	x20, x21, [sp, #-16]!
	stp	x22, x23, [sp, #-16]!
	stp	x24, x25, [sp, #-16]!
	stp	x26, x27, [sp, #-16]!
	stp	x28, x29, [sp, #-16]!
	str x30, [sp, #-8]!

	mrs x0, spsr_el2
	str x0, [sp, #-8]!

	mrs x0, elr_el2
	str x0, [sp, #-8]!

.endm

.global vector
vector:

// ----------------------------------------
sp_el0_sync:        b       .
.align 7 , 0xff // 2^7
sp_el0_irq:         b       .

.align 7 , 0xff // 2^7
sp_el0_fiq:         b       .
    
.align 7 , 0xff // 2^7
sp_el0_serror:      b       .
// ----------------------------------------
.align 7 , 0xff // 2^7
sp_elx_sync:        b       .
    
.align 7 , 0xff // 2^7
sp_elx_irq:
	SAVE_CPU
	bl gic_handle
	b restore_cpu

.align 7 , 0xff // 2^7		
sp_elx_fiq:         b       .
    
.align 7 , 0xff // 2^7
sp_elx_serror:      b       .
// ----------------------------------------
.align 7 , 0xff // 2^7
lower_64_sync:
	VM_EXIT
	bl aborts_sync_hanlder
	b vm_entry
    
.align 7 , 0xff // 2^7
lower_64_irq:
	VM_EXIT
	bl gic_handle
    // bfs 时调用 bl try_reschedule
	bl try_reschedule
	b vm_entry

.align 7 , 0xff // 2^7		 
lower_64_fiq:         b       .
    
.align 7 , 0xff // 2^7
lower_64_serror:      b       .
// ----------------------------------------
.align 7 , 0xff // 2^7
lower_32_sync:        b       .
    
.align 7 , 0xff // 2^7
lower_32_irq:         b       .

.align 7 , 0xff // 2^7
lower_32_fiq:         b       .
    
.align 7 , 0xff // 2^7
lower_32_serror:      b       .
// ----------------------------------------

.global vm_entry
vm_entry:
	mrs x0, tpidr_el2
	ldr x0, [x0, #CPU_VCPU_OFF]
	add x0, x0, #VCPU_REGS_OFF
    // load vcpu_regs
	mov sp, x0

	ldr	x0, [sp, #248]
	msr	spsr_el2, x0

	ldr	x0, [sp , #256]
    msr	elr_el2, x0

    ldr x0,  [sp, #264]
    msr sp_el1, x0

	ldp	x0, x1,   [sp, #0]
	ldp	x2, x3,   [sp, #16]
	ldp	x4, x5,   [sp, #32]
	ldp	x6, x7,   [sp, #48]
	ldp	x8, x9,  [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x16, x17, [sp, #128]
	ldp	x18, x19, [sp, #144]
	ldp	x20, x21, [sp, #160]
	ldp	x22, x23, [sp, #176]
	ldp	x24, x25, [sp, #192]
	ldp	x26, x27, [sp, #208]
	ldp	x28, x29, [sp, #224]
	ldr	x30, [sp, #240]

    eret

.global restore_cpu
restore_cpu:        
    ldr	x0, [sp], #8
    msr	elr_el2, x0 

    ldr	x0, [sp], #8
    msr	spsr_el2, x0

    ldr	x30, 	  [sp], #8 
	ldp	x29, x28, [sp], #16 
	ldp	x27, x26, [sp], #16 
	ldp	x25, x24, [sp], #16 
	ldp	x23, x22, [sp], #16 
	ldp	x21, x20, [sp], #16
	ldp	x19, x18, [sp], #16
	ldp	x17, x16, [sp], #16
	ldp	x15, x14, [sp], #16
	ldp	x13, x12, [sp], #16
	ldp	x11, x10, [sp], #16
	ldp	x9, x8, [sp], #16
	ldp	x7, x6, [sp], #16
	ldp	x5, x4, [sp], #16
	ldp	x3, x2, [sp], #16
	ldp	x1, x0, [sp], #16

    eret
