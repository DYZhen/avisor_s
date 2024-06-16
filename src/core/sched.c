#include "./inc/util.h"
#include "list.h"
#include "./inc/vm.h"
#include "sysregs.h"

// --------------------------------------
// Memory Management
// --------------------------------------

spinlock_t rqnode_memory_lock = SPINLOCK_INITVAL;
spinlock_t vpnode_memory_lock = SPINLOCK_INITVAL;

#define RNODE_NUM (MAX_VM_NUM + 8)
#define VNODE_NUM (MAX_VCPU_NUM + 8)

runQueueNode rqnode_memory[RNODE_NUM];
vCpuNode* multi_queue_memory[RNODE_NUM*PRIO_WIDTH];
vCpuNode vpnode_memory[VNODE_NUM];

BITMAP_ALLOC(rqnode_mem_bitmap, RNODE_NUM);
BITMAP_ALLOC(vpnode_mem_bitmap, VNODE_NUM);

runQueueNode* malloc_rqnode(int* pos) {
    spin_lock(&rqnode_memory_lock);
    *pos = bitmap_find_nth(rqnode_mem_bitmap, RNODE_NUM, 1, 0, 0);
    if (*pos == -1) {
        INFO("error:rqnode_memory full, malloc failed.");
    }
    bitmap_set(rqnode_mem_bitmap, *pos);
    spin_unlock(&rqnode_memory_lock);
    return &(rqnode_memory[*pos]);
}

vCpuNode* malloc_vpnode() {
    spin_lock(&vpnode_memory_lock);
    int pos = bitmap_find_nth(vpnode_mem_bitmap, VNODE_NUM, 1, 0, 0);
    if (pos == -1) {
        INFO("malloc_vpnode error:vpnode_memory full, malloc failed.");
    }
    bitmap_set(vpnode_mem_bitmap, pos);
    spin_unlock(&vpnode_memory_lock);
    return &(vpnode_memory[pos]);
}

void transfer_rq() {
    if (rq_ptr == NULL) {
        INFO("waring:rq_ptr is NULL");
        return ;
    }
    
    runQueueNode* ptr = rq_ptr->next;
    while (ptr != NULL) {
        INFO("VM id : %d", ptr->vm->id);
        INFO("multi_queue == NULL : %d", ptr->multi_queue == NULL);
        INFO("run_vnode == NULL : %d", ptr->run_vnode == NULL);
        INFO("multi_queue");
        for (int i = 0; i < BMQ_BITMAP_SIZE; ++i) {
            ASSERT(ptr->multi_queue != NULL);
            if (ptr->multi_queue[i] == NULL) {
                int bit = (ptr->bitmap >> i) & 1;
                ASSERT(bit == 0);
                INFO("prio %d FIFO queue is NULL", i);
            } else {
                INFO("prio %d", i);
                vCpuNode* vnode = ptr->multi_queue[i];
                while (vnode != NULL) {
                    INFO("vnode id : %d", vnode->cur->id);
                    vnode = vnode->next;
                }
            }
        }
        ptr = ptr->next;
    }
    
    INFO("transfer finish");
}

// ---------------------------------
// BFS
// ---------------------------------

// TODO FINISH 初始化 vm.c:152
int prio_ratios[PRIO_WIDTH];

// TODO FINISH 运行队列初始化 vm.c:153
runQueue* rq_ptr = NULL;
spinlock_t rq_spin_lock = SPINLOCK_INITVAL;

// TODO FINISH BFS 时间片 4s
int bfs_rr_interval = 4000;
uint64_t bfs_clock = 0;
spinlock_t bc_spin_lock = SPINLOCK_INITVAL;

void init_pr() {
    prio_ratios[0] = 128;
    for (int i = 1; i < PRIO_WIDTH; ++i) {
        prio_ratios[i] = (int)(prio_ratios[i-1]*11/10);
    }
}

void p_task_insert(struct vm* vm, uint32_t prio) {
    runQueueNode* rqnode = initialize_rqnode(vm, prio);
    rq_insert(rqnode);
}

runQueueNode* p_task_lookup() {
    spin_lock(&rq_spin_lock);
    runQueueNode* target = NULL;
    runQueueNode* rqnode = rq_ptr->next;
    uint64_t old_timestamp = sysreg_cntpct_el0_read();
    while (rqnode) {
        if (rqnode->nr_vcpu_ready > 0 && (target == NULL || rqnode->vdeadline < target->vdeadline)) {
            target = rqnode;
        }
        rqnode = rqnode->next;
    }
    if (target != NULL) {
        uint64_t new_timestamp = sysreg_cntpct_el0_read();
        INFO("bfs scheduler latency: %u", new_timestamp - old_timestamp);
        INFO("unikernel %d will run on pcpu %d.", target->vm->id, cpu()->id);
        rq_remove(target);
        spin_unlock(&rq_spin_lock);
        return target;
    }
    spin_unlock(&rq_spin_lock);
    return NULL;
}

uint64_t prio_deadline_diff(int user_prio) {
    // TODO 先以毫秒为单位处理
    return (prio_ratios[user_prio] * bfs_rr_interval / 128);
//    return (prio_ratios[user_prio] * bfs_rr_interval * (MS_TO_NS(1) / 128));
}

runQueueNode* initialize_rqnode(struct vm* vm, uint32_t prio) {
    int pos = 0;
    runQueueNode* rqnode = malloc_rqnode(&pos);
    rqnode->prev = rqnode->next = NULL;
    rqnode->vm = vm;
    if (vm != NULL) {
        rqnode->nr_vcpu_ready = vm->nr_cpus;
    }
    rqnode->prio = prio;
    rqnode->vdeadline_diff = prio_deadline_diff(prio);
    rqnode->vdeadline = bfs_clock + rqnode->vdeadline_diff;
    rqnode->bitmap = 0;
    rqnode->multi_queue = &(multi_queue_memory[pos*PRIO_WIDTH]);
    for (int i = 0; i < BMQ_BITMAP_SIZE; ++i) {
        rqnode->multi_queue[i] = NULL;
    }
    rqnode->run_vnode = NULL;
    rqnode->state = TASK_READY;
    rqnode->time_slice = 0;
    rqnode->need_resched = 1;
    // rr
//    rqnode->vq_ptr = initialize_vCpuNode(NULL, 0);
    return rqnode;
}

void rq_insert(runQueueNode* rqnode) {
    // 加锁访问链表
    spin_lock(&rq_spin_lock);
    rqnode->next = rq_ptr->next;
    rqnode->prev = rq_ptr;
    if (rq_ptr->next != NULL) {
        rqnode->next->prev = rqnode;
    }
    rq_ptr->next = rqnode;
    // 解锁访问链表
    spin_unlock(&rq_spin_lock);
}

// 该函数应该只在 p_task_lookup() 中调用
void rq_remove(runQueueNode* rqnode) {
    rqnode->prev->next = rqnode->next;
    if (rqnode->next != NULL) {
        rqnode->next->prev = rqnode->prev;
    }
    rqnode->prev = rqnode->next = NULL;
}

// 切换 VM
static inline void prepare_to_switch(struct vm* vm) {
    sysreg_vttbr_el2_write((((uint64_t)vm->id << VTTBR_VMID_OFF) & VTTBR_VMID_MSK) |
                           ((paddr_t)vm->as.pt.root & ~VTTBR_VMID_MSK));
    ISB();
}

void bfs_scheduler(struct cpu* cur_pcpu) {
    runQueueNode* rqnode = cur_pcpu->rqnode;
    if (rqnode != NULL) {
        ASSERT(rqnode->time_slice == 0);
        // 时间片耗尽，更新虚拟截止时间
        rqnode->vdeadline = bfs_clock + rqnode->vdeadline_diff;
        // 将任务插入就绪队列
        rqnode->state = TASK_READY;
        rqnode->need_resched = 1;
        rq_insert(rqnode);
    }
    
    // 选择新的任务
    rqnode = NULL;
    while (rqnode == NULL) {
        rqnode = p_task_lookup();
    }
    rqnode->state = TASK_RUNNING;
    // TODO 先以毫秒为单位处理
    rqnode->time_slice = (bfs_rr_interval);
    rqnode->need_resched = 0;
    cur_pcpu->rqnode = rqnode;
    rqnode->vm->master = cur_pcpu->id;
    prepare_to_switch(rqnode->vm);
}

// ------------------------------
// BMQ
// ------------------------------

// TODO FINISH BMQ 轮转时间为 1s
int bmq_rr_interval = 1000;

vCpuNode* initialize_vCpuNode(struct vcpu* vp, uint32_t static_prio) {
    vCpuNode * vpnode = malloc_vpnode();
    vpnode->cur = vp;
    vpnode->next = NULL;
    vpnode->static_prio = static_prio;
    vpnode->prio_adj = 0;
    vpnode->prio = static_prio;
    vpnode->state = TASK_READY;
    vpnode->time_slice = 0;
    vpnode->need_resched = 1;
    return vpnode;
}

void v_task_insert(vCpuNode* vnode, runQueueNode* rqnode) {
    // 将位图置位
    uint64_t mask = 1ULL << vnode->prio;
    rqnode->bitmap |= mask;
    // 插入响应的队列
    vCpuNode* vnptr = rqnode->multi_queue[vnode->prio];
    if (vnptr == NULL) {
        rqnode->multi_queue[vnode->prio] = vnode;
        rqnode->multi_queue[vnode->prio]->next = NULL;
    } else {
        while (vnptr->next != NULL) {
            vnptr = vnptr->next;
        }
        vnptr->next = vnode;
        vnode->next = NULL;
    }
    rqnode->nr_vcpu_ready ++;
}

vCpuNode* v_task_lookup(runQueueNode* rqnode) {
    uint64_t old_timestamp = sysreg_cntpct_el0_read();
    for (int i = 0; i < BMQ_BITMAP_SIZE; ++i) {
        uint64_t bit = (rqnode->bitmap >> i) & 1ULL;
        if (bit == 1) {
            uint64_t new_timestamp = sysreg_cntpct_el0_read();
            INFO("bmq scheduler latency: %u", new_timestamp - old_timestamp);
            vCpuNode* vnode = rqnode->multi_queue[i];
            // 当前位图对应的队列中最后一个 vcpu 被调度
            if (vnode->next == NULL) {
                // 将位图相应位置零
                uint64_t mask = 1ULL << i;
                rqnode->bitmap &= ~mask;
            }
            // 被选中的 vcpu 从队列中删除
            rqnode->multi_queue[i] = vnode->next;
            rqnode->nr_vcpu_ready--;
            vnode->next = NULL;
            INFO("vcpu:prio (%d:%d) run on unikernel %d on pcpu %d.", vnode->cur->id, vnode->prio,
                   vnode->cur->vm->id, cpu()->id);
            return vnode;
        }
    }
//    INFO("error:no task ready.");
    return NULL;
}

void prio_manage(vCpuNode* vnode, int up_or_down) {
    if (up_or_down == 1 && vnode->prio_adj > -MAXPRIO_ADJ) {
        vnode->prio_adj -= 1;
    } else if (vnode->prio_adj < MAXPRIO_ADJ) {
        vnode->prio_adj += 1;
    }
    int temp_prio = vnode->static_prio + vnode->prio_adj;
    if ((temp_prio <= PRIO_WIDTH-1) && (temp_prio >= 0)) {
        vnode->prio = temp_prio;
    }
}

// TODO FINISH 切换 VCPU
static inline void switch_to(vCpuNode* __next) {
    struct cpu* cur_pcpu = cpu();
    struct vcpu* vcpu = __next->cur;
    cur_pcpu->vcpu = vcpu;
    vcpu->p_id = cur_pcpu->id;
    sysreg_vmpidr_el2_write(vcpu->arch.vmpidr);
}

// TODO FINISH 何时处理外部中断来临后 VCPU 的优先级提升
void bmq_scheduler(runQueueNode* rqnode) {
    vCpuNode* vnode = rqnode->run_vnode;
    if (vnode != NULL) {
        ASSERT(vnode->time_slice == 0);
        // 当前 uk 被调度过，其上 vcpu 之前运行过，需要进行处理
        // 否则直接重新调度即可
        // 时间片耗尽，优先级降低
        if (sleep_vnode == NULL || vnode != sleep_vnode) {
            prio_manage(vnode, 0);
            vnode->state = TASK_READY;
            vnode->need_resched = 1;
            // 将 vcpu 重新插入多级队列
            v_task_insert(vnode, rqnode);
        }
    }
    // 重新调度
    vnode = NULL;
    while (vnode == NULL) {
        vnode = v_task_lookup(rqnode);
    }
    vnode->state = TASK_RUNNING;
    vnode->time_slice = (bmq_rr_interval);
    vnode->need_resched = 0;
    rqnode->run_vnode = vnode;
    switch_to(vnode);
}

void try_reschedule() {
    struct cpu* cur_pcpu = cpu();
    if (cur_pcpu->flag == 1) {
        return;
    } else {
        cur_pcpu->flag = 1;
    }
    runQueueNode* current_vm = cur_pcpu->rqnode;
    if (current_vm == NULL || current_vm->need_resched == 1) {
        // current == NULL 用于处理系统初始状态
        INFO("VM need reschedule, call bfs_scheduler and bmq_scheduler cpu:%d", cpu()->id);
        bfs_scheduler(cur_pcpu);
        bmq_scheduler(cur_pcpu->rqnode);
    } else if (current_vm->run_vnode == NULL || current_vm->run_vnode->need_resched == 1) {
        INFO("VCPU need reschedule, call bmq_scheduler cpu:%d", cpu()->id);
        bmq_scheduler(current_vm);
    }
    cur_pcpu->flag = 0;
    
    uint64_t timestamp = sysreg_cntpct_el0_read();
    INFO("TIME: %u vm id %d vcpu id %d pcpu id %d", timestamp, cur_pcpu->rqnode->vm->id, cur_pcpu->vcpu->id, cur_pcpu->id);
}

extern uint64_t TIMER_WAIT;

void rr_try_reschedule(int vm_reschedule, int vcpu_reschedule) {
    if (vm_reschedule == 1) {
        rr_scheduler_vm(cpu());
        rr_scheduler_vcpu(cpu()->rqnode);
    } else if (vcpu_reschedule == 1) {
        rr_scheduler_vcpu(cpu()->rqnode);
    }
    
    uint64_t timestamp = sysreg_cntpct_el0_read();
    INFO("TIME: %u vm id %d vcpu id %d pcpu id %d", timestamp, cpu()->rqnode->vm->id, cpu()->vcpu->id, cpu()->id);
}

// TODO 先以毫秒为单位处理
void update_task_times() {
    runQueueNode* current_vm = cpu()->rqnode;
    vCpuNode* current_vcpu = current_vm->run_vnode;
    
    if (cpu()->id == 0) {
        bfs_clock += (TIMER_WAIT);
        if (io_sleep_flag > 0) {
            if (--io_sleep_flag == 0) {
                // bfs
                sleep_vnode->need_resched = 1;
                v_task_insert(sleep_vnode, with_sleep_vnode_rqnode);
                // rr
//                rr_vnode_insert(sleep_vnode, with_sleep_vnode_rqnode);
                sleep_vnode = NULL;
                with_sleep_vnode_rqnode = NULL;
            }
        }
    }
    
    cpu_sync_barrier(&cpu_glb_sync);
    
    int vm_reschedule = 0;
    int vcpu_reschedule = 0;
    
    if (current_vcpu->time_slice <= (TIMER_WAIT)) {
        current_vcpu->time_slice = 0;
        current_vcpu->need_resched = 1;
        // rr
//        vcpu_reschedule = 1;
    } else {
        current_vcpu->time_slice -= (TIMER_WAIT);
    }
    
    if (current_vm->time_slice <= (TIMER_WAIT)) {
        current_vm->time_slice = 0;
        current_vm->need_resched = 1;
        // rr
//        vm_reschedule = 1;
    } else {
        current_vm->time_slice -= (TIMER_WAIT);
    }
    
//    rr_try_reschedule(vm_reschedule, vcpu_reschedule);
}

runQueueNode* rr_vm_lookup() {
    runQueueNode* target = NULL;
    spin_lock(&rq_spin_lock);
    uint64_t old_timestamp = sysreg_cntpct_el0_read();
    if (rq_ptr->next == NULL) {
        INFO("error:no task in runqueue.");
    } else {
        target = rq_ptr->next;
        // remove
        rq_ptr->next = target->next;
    }
    uint64_t new_timestamp = sysreg_cntpct_el0_read();
    INFO("rr_scheduler_vm latency: %u", new_timestamp - old_timestamp);
    spin_unlock(&rq_spin_lock);
    return target;
}

void rr_scheduler_vm(struct cpu* cur_pcpu) {
    runQueueNode* rqnode = cur_pcpu->rqnode;
    if (rqnode != NULL) {
        ASSERT(rqnode->time_slice == 0);
        rr_rq_insert(rqnode);
    }
    
    // 选择新的任务
    rqnode = NULL;
    while (rqnode == NULL) {
        rqnode = rr_vm_lookup();
    }
    rqnode->state = TASK_RUNNING;
    // TODO 先以毫秒为单位处理
    rqnode->time_slice = (bfs_rr_interval);
    rqnode->need_resched = 0;
    cur_pcpu->rqnode = rqnode;
    prepare_to_switch(rqnode->vm);
}

vCpuNode* rr_vcpu_lookup(runQueueNode* rqnode) {
    uint64_t old_timestamp = sysreg_cntpct_el0_read();
    if (rqnode->vq_ptr->next == NULL) {
        INFO("error:no task in vcpu runqueue.");
        return NULL;
    } else {
        vCpuNode* target = rqnode->vq_ptr->next;
        // remove
        rqnode->vq_ptr->next = target->next;
        uint64_t new_timestamp = sysreg_cntpct_el0_read();
        INFO("rr_scheduler_vcpu latency: %u", new_timestamp - old_timestamp);
        return target;
    }
}

void rr_scheduler_vcpu(runQueueNode* rqnode) {
    vCpuNode* vnode = rqnode->run_vnode;
    if (vnode != NULL) {
        ASSERT(vnode->time_slice == 0);
        if (sleep_vnode == NULL || vnode != sleep_vnode) {
            // 将 vcpu 重新插入队列
            rr_vnode_insert(vnode, rqnode);
        }
        
    }
    // 重新调度
    
    vnode = NULL;
    while (vnode == NULL) {
        vnode = rr_vcpu_lookup(rqnode);
    }
    vnode->state = TASK_RUNNING;
    // TODO 先以毫秒为单位处理
    vnode->time_slice = (bmq_rr_interval);
    vnode->need_resched = 0;
    rqnode->run_vnode = vnode;
    switch_to(vnode);
}

void rr_rq_insert(runQueueNode* rqnode) {
    spin_lock(&rq_spin_lock);
    runQueueNode* ptr = rq_ptr->next;
    runQueueNode* prev = rq_ptr;
    while(ptr) {
        ptr = ptr->next;
        prev = prev->next;
    }
    rqnode->next = prev->next;
    prev->next = rqnode;
    spin_unlock(&rq_spin_lock);
}

void rr_vnode_insert(vCpuNode* vnode, runQueueNode* rqnode) {
    vCpuNode* ptr = rqnode->vq_ptr->next;
    vCpuNode* prev = rqnode->vq_ptr;
    while(ptr) {
        ptr = ptr->next;
        prev = prev->next;
    }
    vnode->next = prev->next;
    prev->next = vnode;
    rqnode->nr_vcpu_ready++;
}

void sched_start_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    struct vcpu* target_vcpu = &(cpu()->rqnode->vm->vcpus[arg0]);
    memcpy(&(target_vcpu->regs), &cpu()->vcpu->regs, sizeof(struct vcpu_regs));
    vcpu_writepc(target_vcpu, arg1);
    v_task_insert(initialize_vCpuNode(target_vcpu, bmq_default_prio), cpu()->rqnode);
    // rr
//    rr_vnode_insert(initialize_vCpuNode(target_vcpu, bmq_default_prio), cpu()->rqnode);
}

int io_sleep_flag = 0;

vCpuNode* sleep_vnode = NULL;
runQueueNode* with_sleep_vnode_rqnode = NULL;

void sched_io_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    
    with_sleep_vnode_rqnode = cpu()->rqnode;
    sleep_vnode = with_sleep_vnode_rqnode->run_vnode;
    sleep_vnode->need_resched = 0;
    // 提升优先级
    prio_manage(sleep_vnode, 1);
    // 设置全局变量的值
    io_sleep_flag = 4;
    
    // rr 额外的什么都不需要做
}