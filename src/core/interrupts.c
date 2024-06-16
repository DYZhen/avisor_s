#include "interrupts.h"
#include "bitmap.h"
#include "arch_vm.h"
#include "vm.h"
#include "timer.h"
#include "sched.h"

BITMAP_ALLOC(hyp_interrupt_bitmap, MAX_INTERRUPTS);
BITMAP_ALLOC(global_interrupt_bitmap, MAX_INTERRUPTS);

extern void uart_handler(void);

irq_handler_t interrupt_handlers[MAX_INTERRUPTS];

inline void interrupts_cpu_enable(irqid_t int_id, bool en) {
    interrupts_arch_enable(int_id, en);
}

inline void interrupts_cpu_sendipi(cpuid_t target_cpu, irqid_t ipi_id) {
    interrupts_arch_ipi_send(target_cpu, ipi_id);
}

inline bool interrupts_check(irqid_t int_id) {
    return interrupts_arch_check(int_id);
}

inline void interrupts_clear(irqid_t int_id) {
    interrupts_arch_clear(int_id);
}

static inline bool interrupt_is_reserved(irqid_t int_id) {
    return bitmap_get(hyp_interrupt_bitmap, int_id);
}

void interrupts_reserve(irqid_t int_id, irq_handler_t handler) {
    if (int_id < MAX_INTERRUPTS) {
        interrupt_handlers[int_id] = handler;
        bitmap_set(hyp_interrupt_bitmap, int_id);
        bitmap_set(global_interrupt_bitmap, int_id);
    }
}

enum irq_res interrupts_handle(irqid_t int_id) {
//     INFO("Enter %s id = %d", __func__, int_id);

    if (vm_has_interrupt(cpu()->vcpu->vm, int_id)) {
        // INFO("Forward %d to vm", int_id);
        vcpu_inject_hw_irq((cpu()->vcpu), int_id);

        return FORWARD_TO_VM;

    } else if (interrupt_is_reserved(int_id)) {
        // INFO("Forward %d to hypervisor", int_id);
        interrupt_handlers[int_id](int_id);
        return HANDLED_BY_HYP;

    } else {
        ERROR("received unknown interrupt id = %d", int_id);
    }
}

void interrupts_vm_assign(struct vm *vm, irqid_t id) {
    if (interrupts_arch_conflict(global_interrupt_bitmap, id)) {
        ERROR("Interrupts conflict, id = %d\n", id);
    }

    interrupts_arch_vm_assign(vm, id);

    bitmap_set(vm->interrupt_bitmap, id);
    bitmap_set(global_interrupt_bitmap, id);
}

static void timer_interrupt_handler() {
    timer_handler();
//#ifdef SCHEDULE
    update_task_times();
//#endif
}

static inline void physicl_timer_init() {
    // enable intid = 26 physical timer
    INFO("Timer Init");
    timer_init();
    interrupts_reserve(IRQ_TIMER, timer_interrupt_handler);
    
    if (interrupt_is_reserved(IRQ_TIMER)) {
        INFO("Reserved %d intr", IRQ_TIMER);
    }

    gic_set_prio(IRQ_TIMER, 0);
    gic_set_pend(IRQ_TIMER, false);
    gic_set_enable(IRQ_TIMER, true);
}

void interrupts_init() {
    interrupts_arch_init();

    if (cpu()->id == CPU_MASTER) {
        interrupts_reserve(IPI_CPU_MSG, cpu_msg_handler);
    }

    physicl_timer_init();
    
    interrupts_cpu_enable(IPI_CPU_MSG, true);

    INFO("CPU[%d] INTERRUPT INIT", cpu()->id);
}