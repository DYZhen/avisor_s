#ifndef SCHED_H
#define SCHED_H

#include "types.h"
#include "../../lib/inc/spinlock.h"
#include "bitmap.h"

#define DEFAULT_COUNTER 2
#define BMQ_LEVELS 8
#define DEFAULT_LEVEL 4

#define TASK_READY       0
#define TASK_RUNNING     1
#define TASK_PENDING     2

extern struct vm;

void try_reschedule();
void update_task_times();
void schedule();

#define bfs_default_prio 5
#define bmq_default_prio 5

// -----------------------
// BFS
// -----------------------

#define PRIO_WIDTH 10

/* 一些用于转换到各种比例/从各种比例转换的助手。使用移位获得10的近似倍数，以减少开销。*/
#define JIFFIES_TO_NS(TIME)	((TIME) * (1000000000 / HZ))
#define JIFFY_NS		(1000000000 / HZ)
#define HALF_JIFFY_NS		(1000000000 / HZ / 2)
#define HALF_JIFFY_US		(1000000 / HZ / 2)
#define MS_TO_NS(TIME)		((TIME) << 20)
#define MS_TO_US(TIME)		((TIME) << 10)
#define NS_TO_MS(TIME)		((TIME) >> 20)
#define NS_TO_US(TIME)		((TIME) >> 10)

// 时间片长度，单位为毫秒
extern int bfs_rr_interval;
// The relative length of deadline for each priority(nice) level.
extern int prio_ratios[];

void init_pr(); // 初始化优先级偏移查找表
// 计算得到 vdeadline 中优先级偏移部分
uint64_t prio_deadline_diff(int user_prio);

extern struct vcpunode;

// 任务队列
typedef struct runqueue_node
{
    struct vm* vm;
    uint32_t nr_vcpu_ready; // 未投入运行的就绪 vcpu 数量
    uint32_t prio; // 0-39
    uint64_t vdeadline_diff;
    uint64_t vdeadline;
    struct runqueue_node* prev;
    struct runqueue_node* next;
    // bmqMultiQueue
    uint64_t bitmap;
    struct vcpunode** multi_queue;
    struct vcpunode* run_vnode; // 当前运行的 vcpu
    int state;
    uint64_t time_slice;    // 当前VM剩余可用时间片
    uint32_t need_resched; // 当前任务是否需要被重新调度
    struct vcpunode* vq_ptr;    // rr 调度中的任务队列
} runQueue, runQueueNode;

extern runQueue* rq_ptr;
extern spinlock_t rq_spin_lock;

runQueueNode* initialize_rqnode(struct vm* vm, uint32_t prio);
void rq_insert(runQueueNode* rqnode);
void rq_remove(runQueueNode* rqnode);
void p_task_insert(struct vm* vm, uint32_t prio);
runQueueNode* p_task_lookup();
void transfer_rq();

/*
 * 时钟，用于记录系统运行的纳秒数
 * 更新时间：
 * 1.某一 pcpu 完成当前任务
 * 2.某一 pcpu 任务中断
 * 3.有新的任务进入系统
 *
 * 为了防止溢出，可以考虑采用秒+毫秒+纳秒的计数方案
 */
extern uint64_t bfs_clock;
// 时钟互斥锁
extern spinlock_t bc_spin_lock;
extern struct cpu;

// TODO FINISH 如何处理 PCPU 的初始化：在 cpu.c 的 cpu_init 函数部分
//pCpu* initialize_pcpu();

// bfs 调度逻辑
void bfs_scheduler(struct cpu* cur_pcpu);

// ---------------------------
// BMQ
// ---------------------------

// 时间片长度，单位为毫秒
extern int bmq_rr_interval;

#define MAXPRIO_ADJ 5

typedef struct vcpunode {
    struct vcpu* cur;
    uint32_t static_prio;
    int prio_adj;
    uint32_t prio;
    struct vcpunode* next;
    int state;
    uint64_t time_slice;    // 当前VCPU剩余可用时间片，单位为纳秒
    uint32_t need_resched; // 当前VCPU是否需要被重新调度
} vCpuNode;

// TODO FINISH 如何处理VCPU的初始化：vm.c vm_vcpu_init
// vCpu* initialize_vCpu(int vid);
vCpuNode* initialize_vCpuNode(struct vcpu* vp, uint32_t static_prio);

#define BMQ_BITMAP_SIZE 10

void v_task_insert(vCpuNode* vnode, runQueueNode* rqnode);
vCpuNode* v_task_lookup(runQueueNode* rqnode);
void prio_manage(vCpuNode* vnode, int up_or_down);
void bmq_scheduler(runQueueNode* rqnode);

// Memory management

extern spinlock_t rqnode_memory_lock;
extern spinlock_t vpnode_memory_lock;
extern runQueueNode rqnode_memory[];
extern vCpuNode* multi_queue_memory[];
extern vCpuNode vpnode_memory[];
extern bitmap_t rqnode_mem_bitmap[];
extern bitmap_t vpnode_mem_bitmap[];

runQueueNode* malloc_rqnode(int* pos);
vCpuNode* malloc_vpnode();

// 轮转调度算法，用于实验对照
// vm 调度算法可以直接使用 bfs 的运行队列

runQueueNode* rr_vm_lookup();
void rr_scheduler_vm(struct cpu* cur_pcpu);
vCpuNode* rr_vcpu_lookup(runQueueNode* rqnode);
void rr_scheduler_vcpu(runQueueNode* rqnode);
void rr_try_reschedule(int vm_reschedule, int vcpu_reschedule);
void rr_rq_insert(runQueueNode* rqnode);
void rr_vnode_insert(vCpuNode* vnode, runQueueNode* rqnode);


extern int io_sleep_flag;
extern vCpuNode* sleep_vnode;
extern runQueueNode* with_sleep_vnode_rqnode;
// hypercall handler
void sched_start_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);
void sched_io_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);



#endif