#ifndef CPU_H
#define CPU_H

#include "util.h"
#include "arch_cpu.h"
#include "atomic.h"
#include "fences.h"
#include "list.h"
#include "spinlock.h"

#include "sched.h"

#define CPU_MASTER 0

struct cpuif {
    struct list_head event_list;
    spinlock_t event_list_lock;
} __attribute__((aligned(PAGE_SIZE))) ;

struct vcpu;

struct cpu {
    cpuid_t id;
    bool handling_msgs;
    uint8_t pad[7];
    struct vcpu* vcpu;
    struct cpu_arch arch;
    struct cpuif* interface;
    
    runQueueNode* rqnode; // 正在运行的 rqnode
    int flag;  //   标记是否正在 try_reschedule
//    uint64_t pcpu_clock;    // 计时器，单位为纳秒
//     TODO 是否需要将以下部分放到 VCPU 中备份
    uint8_t stack[STACK_SIZE] __attribute__((aligned(PAGE_SIZE)));
} __attribute__((aligned(PAGE_SIZE)));

struct cpu_msg {
    uint32_t handler;
    uint32_t event;
    uint64_t data;
};

void cpu_send_msg(cpuid_t cpu, struct cpu_msg* msg);
void cpu_msg_handler();

typedef void (*cpu_msg_handler_t)(uint32_t event, uint64_t data);

#define CPU_MSG_HANDLER(handler, handler_id)                    \
    __attribute__((section(".ipi_cpumsg_handlers"), used))      \
        cpu_msg_handler_t __cpumsg_handler_##handler = handler; \
    __attribute__((section(".ipi_cpumsg_handlers_id"),          \
                   used)) volatile const size_t handler_id;

struct cpu_synctoken {
    volatile bool ready;
    int n;
    atomic_t count;
};

extern struct cpu_synctoken cpu_glb_sync;

extern struct cpuif cpu_interfaces[];
static inline struct cpuif* cpu_if(cpuid_t cpu_id) {
    return &cpu_interfaces[cpu_id];
}

static inline void cpu_sync_init(struct cpu_synctoken* token, size_t n) {
    token->n = n;
    atomic_set(&token->count, 0);
    fence_ord_write();
    token->ready = true;
    fence_ord_write();
}

static inline void cpu_sync_barrier(struct cpu_synctoken* token) {
    int val, exp_v;
    
    while (!token->ready);
    val = atomic_inc_return(&token->count);
    exp_v = ((val - 1) / token->n + 1) * token->n;

    while (atomic_read(&token->count) < exp_v);
}

static inline void cpu_sync_and_clear_msgs(struct cpu_synctoken* token) {
    cpu_sync_barrier(token);
    size_t next_count = 0;

    while (!token->ready);
    atomic_add(1, &token->count);
    next_count = ALIGN(atomic_read(&token->count), token->n);

    while (atomic_read(&token->count) < next_count) {
        if (!cpu()->handling_msgs) cpu_msg_handler();
    }

    if (!cpu()->handling_msgs) cpu_msg_handler();

    cpu_sync_barrier(token);
}

void cpu_init(cpuid_t cpu_id);
void cpu_arch_init(cpuid_t cpu_id);

void cpu_send_msg(cpuid_t cpu, struct cpu_msg* msg);
bool cpu_get_msg(struct cpu_msg* msg);
void cpu_msg_set_handler(cpuid_t id, cpu_msg_handler_t handler);

#endif