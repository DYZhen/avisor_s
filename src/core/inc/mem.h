#ifndef MEM_H
#define MEM_H

#include "util.h"
#include "mmu.h"
#include "list.h"
#include "spinlock.h"
#include "bitmap.h"

struct ppages {
    paddr_t base;
    size_t nr_pages;
};

struct page_pool {
    paddr_t base;
    size_t nr_pages;
    size_t free;
    size_t last;
    bitmap_t* bitmap;
    spinlock_t lock;
};

struct mem_region {
    paddr_t base;
    size_t size;
    struct page_pool page_pool;
};

static inline struct ppages mem_ppages_get(paddr_t base, size_t nr_pages) {
    return (struct ppages){.base = base, .nr_pages = nr_pages};
}

void mem_init();
void* mem_alloc_page(size_t nr_pages, bool phys_aligned);
struct ppages mem_alloc_ppages(size_t nr_pages, bool aligned);
bool root_pool_set_up_bitmap(struct page_pool *root_pool);
bool pp_alloc(struct page_pool *pool, size_t nr_pages, bool aligned,
                     struct ppages *ppages);
vaddr_t mem_alloc_map(struct addr_space* as, struct ppages *page, 
                        vaddr_t at, size_t nr_pages, mem_flags_t flags);
bool mem_map(struct addr_space *as, vaddr_t va, struct ppages *ppages,
            size_t nr_pages, mem_flags_t flags);                 
vaddr_t mem_alloc_map_dev(struct addr_space* as, 
                             vaddr_t at, paddr_t pa, size_t nr_pages);  
void mem_unmap(struct addr_space* as, vaddr_t at, size_t num_pages,
                    bool free_ppages);     
/* Functions implemented in architecture dependent files */

void as_arch_init(struct addr_space* as);
bool mem_translate(struct addr_space* as, vaddr_t va, paddr_t* pa);

extern struct page_pool* root_page_pool;

#endif /* MEM_H */
