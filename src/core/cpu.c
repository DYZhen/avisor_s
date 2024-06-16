#include "util.h"
#include "cpu.h"
#include "config.h"
#include "objpool.h"
#include "interrupts.h"
#include "platform.h"
#include "list.h"
#include "spinlock.h"

struct cpu_msg_node {
    struct list_head list;
    struct cpu_msg msg;
};

#define CPU_MSG_POOL_SIZE_DEFAULT (128)
#ifndef CPU_MSG_POOL_SIZE
#define CPU_MSG_POOL_SIZE CPU_MSG_POOL_SIZE_DEFAULT
#endif

OBJPOOL_ALLOC(msg_pool, struct cpu_msg_node, CPU_MSG_POOL_SIZE);

struct cpu_synctoken cpu_glb_sync = {.ready = false};


extern uint8_t _ipi_cpumsg_handlers_size;
extern cpu_msg_handler_t ipi_cpumsg_handlers[];
extern size_t _ipi_cpumsg_handlers_id_start[];
size_t ipi_cpumsg_handler_num;

// 暂时
#define PLAT_CPU_NUM  2
struct cpuif cpu_interfaces[PLAT_CPU_NUM];

void cpu_init(cpuid_t cpu_id) {
    cpu()->id = cpu_id;
    cpu()->handling_msgs = false;
    cpu()->interface = cpu_if(cpu()->id);
    cpu()->rqnode = NULL;
    cpu()->flag = 0;
//    cpu()->pcpu_clock = 0;

    cpu_arch_init(cpu_id);

    INIT_LIST_HEAD(&cpu()->interface->event_list);
    cpu()->interface->event_list_lock = SPINLOCK_INITVAL;

    if (cpu()->id == CPU_MASTER) {
        cpu_sync_init(&cpu_glb_sync, config.hyp.nr_cpus);

        ipi_cpumsg_handler_num = ((size_t)&_ipi_cpumsg_handlers_size) / sizeof(cpu_msg_handler_t);
        for (size_t i = 0; i < ipi_cpumsg_handler_num; i++) {
            ((size_t*)_ipi_cpumsg_handlers_id_start)[i] = i;
        }
    }

    INFO("CPU[%d] INIT", cpu_id);
    cpu_sync_barrier(&cpu_glb_sync);
}

void cpu_idle() {
    asm volatile("wfi");
}

void cpu_send_msg(cpuid_t trgtcpu, struct cpu_msg *msg) {
    struct cpu_msg_node *node = objpool_alloc(&msg_pool);

    if (node == NULL) {
        ERROR("cant allocate msg node");
    }
    node->msg = *msg;
    // list_push(&cpu_if(trgtcpu)->event_list, (node_t *)node);
    list_add_tail(&node->list, &cpu_if(trgtcpu)->event_list);
    fence_sync_write();
    interrupts_cpu_sendipi(trgtcpu, IPI_CPU_MSG);
}


bool cpu_get_msg(struct cpu_msg *msg) {
    struct cpu_msg_node *node = NULL;
    // if ((node = (struct cpu_msg_node *)list_pop(&cpu()->interface->event_list)) !=
    //     NULL) {
    //     *msg = node->msg;
    //     objpool_free(&msg_pool, node);
    //     return true;
    // }
    if (!list_empty(&cpu()->interface->event_list)) {
        spin_lock(&cpu()->interface->event_list_lock);
        node = (struct cpu_msg_node *)list_last_entry(&cpu()->interface->event_list, struct cpu_msg_node, list);
        spin_unlock(&cpu()->interface->event_list_lock);
        *msg = node->msg;
        list_del(&node->list);
        objpool_free(&msg_pool, node);
        return true;
    }
    return false;
}


void cpu_msg_handler() {
    cpu()->handling_msgs = true;
    struct cpu_msg msg;
    
    while (cpu_get_msg(&msg)) {
        if (msg.handler < ipi_cpumsg_handler_num &&
            ipi_cpumsg_handlers[msg.handler]) {
            ipi_cpumsg_handlers[msg.handler](msg.event, msg.data);
        }
    }
    cpu()->handling_msgs = false;
}