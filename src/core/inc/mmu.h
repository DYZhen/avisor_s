#ifndef MMU_H
#define MMU_H

#include "util.h"
#include "arch_mem.h"
#include "spinlock.h"
#include "page_table.h"

#define HYP_ASID 0

struct addr_space {
    struct page_table pt;
    enum type type;
    asid_t asid;
    spinlock_t lock;
};
enum AS_SEC;

typedef pte_t mem_flags_t;

void as_init(struct addr_space* as, enum type type, asid_t asid,
            pte_t* root_pt);
vaddr_t mem_alloc_vpage(struct addr_space* as, vaddr_t at, size_t n);

#endif