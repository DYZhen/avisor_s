#include "lcm.h"
#include "string.h"

struct snapshot* latest_ss;
ssid_t latest_ss_id = 0;
struct list_head ss_pool_list;
size_t current_restore_cnt = 0;

static inline struct snapshot* get_new_ss() {
    struct snapshot_pool* ss_pool = list_last_entry(&ss_pool_list, struct snapshot_pool, list);
    //TODO: check if enough
    return (struct snapshot*) ss_pool->last;
}

static inline void update_ss_pool_last(size_t size) {
    struct snapshot_pool* ss_pool = list_last_entry(&ss_pool_list, struct snapshot_pool, list);
    latest_ss = (struct snapshot*) ss_pool->last;
    ss_pool->last += size;
}

static struct snapshot_pool* alloc_ss_pool() {
    struct snapshot_pool* ss_pool;
    size_t pool_size = (config.vm->dmem_size + sizeof(struct snapshot)) * 5;

    INFO("new snapshot pool size: %dMB", pool_size / 1024 / 1024);
    
    ss_pool = (struct snapshot_pool*) mem_alloc_page(NUM_PAGES(pool_size), false);
    ss_pool->base = (paddr_t) ss_pool + sizeof(struct snapshot_pool);
    ss_pool->size = pool_size;
    ss_pool->last = ss_pool->base;
    INIT_LIST_HEAD(&ss_pool->list);

    return ss_pool;
}

void ss_pool_init() {
    INIT_LIST_HEAD(&ss_pool_list);
    struct snapshot_pool* ss_pool = alloc_ss_pool();
    list_add_tail(&ss_pool->list, &ss_pool_list);
}

static inline ssid_t get_new_ss_id() {
    return latest_ss_id++;
}

void checkpoint_snapshot_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    static bool init = false;
    struct snapshot* ss;
    paddr_t pa;
    const struct vm_config* config = CURRENT_VM->vm_config;

    if (!init) {
        ss_pool_init();
        init = true;
    }

    ss = get_new_ss();
    if (ss == NULL) {
        ERROR("Not enough snapshot");
    }

    ss->ss_id = get_new_ss_id();
    ss->size = config->dmem_size + config->dmem_size + sizeof(struct snapshot);
    ss->vm_id = CURRENT_VM->id;
    memcpy(&ss->vcpu, cpu()->vcpu, sizeof(struct vcpu));
//    ss->vcpu.regs.sp_el1 = sysreg_sp_el1_read();
    mem_translate(&CURRENT_VM->as, config->base_addr, &pa);
    memcpy(ss->mem, (void*)pa, config->size + config->dmem_size);

    update_ss_pool_last(ss->size);

    vcpu_writereg(cpu()->vcpu, 0, ss->ss_id);
    INFO("checkpoint snapshot over.");
}

static inline struct snapshot* get_latest_ss() {
    return latest_ss;
}

void restart_vm() {
    paddr_t pa;
    const struct vm_config* config = CURRENT_VM->vm_config;

    mem_translate(&CURRENT_VM->as, config->base_addr, &pa);
    memset((void*)pa, 0, config->dmem_size + config->dmem_size);
    memcpy((void*)pa, (void*)config->load_addr, config->dmem_size);

    // Set entry address
    vcpu_arch_reset(cpu()->vcpu, config->entry);
}

void restore_snapshot_hanlder_by_ss(struct snapshot* ss) {
    paddr_t pa;
    const struct vm_config* config = CURRENT_VM->vm_config;

//    sysreg_sp_el1_write(ss->vcpu.regs.sp_el1);
    memcpy(cpu()->vcpu, &ss->vcpu, sizeof(struct vcpu));
    mem_translate(&CURRENT_VM->as, config->base_addr, &pa);
    memcpy((void*)pa, ss->mem, config->dmem_size + config->dmem_size);
}

static inline struct snapshot* get_ss_by_id(ssid_t id) {
    paddr_t ss;
    struct snapshot_pool* ss_pool;
    int i = 0;

    list_for_each_entry(ss_pool, &ss_pool_list, list) {
        ss = ss_pool->base;
        while (i < id && ss + ((struct snapshot*)ss)->size <= ss_pool->base + ss_pool->size) {
            ss += ((struct snapshot*)ss)->size;
            i++;
        }
        if (id == i) {
            return (struct snapshot*) ss;
        }
    }

    ERROR("invalid snapshot id");
}

void restore_snapshot_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) { 
    ssid_t ssid = arg0;
    // ssid_t ssid = CURRENT_VM->vcpu0.regset.x1;
    struct snapshot* ss;
    static bool first = true;

    INFO("restore ssid = %ld", ssid);
    if (ssid == LATEST_SSID) {
        INFO("Restore latest snapshot");
        ss = get_latest_ss();

        if (ss == NULL) {
            INFO("No latest snapshot.");
            restart_vm();
        } else {
            restore_snapshot_hanlder_by_ss(ss);
        }
    } else {
        if (!first) {
            INFO("end");
            while(1);
        }
        ss = get_ss_by_id(ssid);

        if (ss == NULL) {
            ERROR("No no.%d snapshot.", ssid);
        }
        restore_snapshot_hanlder_by_ss(ss);
        first = false;
    }
}

void guest_halt_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    unsigned long reason = arg0;

    switch (reason) {
        case PSCI_FNID_SYSTEM_OFF:
            INFO("Guest System off %d", cpu()->id);
            break;
        case PSCI_FNID_SYSTEM_RESET: {
            if (current_restore_cnt < NUM_MAX_SNAPSHOT_RESOTRE) {
                INFO("Try to restore latest snapshot.");
                current_restore_cnt++;
                restore_snapshot_hanlder(iss, arg0, arg1, arg2);
            } else {
                ERROR("Reach maximum number of restores.")
            }
            break;
        }
    }
}