#include "util.h"
#include "mem.h"
#include "fences.h"
#include "string.h"

struct page_pool* root_page_pool;
extern uint8_t __mem_vm_begin, __mem_vm_end;

bool root_pool_set_up_bitmap(struct page_pool *root_pool) {
    size_t bitmap_nr_pages, bitmap_base, pageoff;
    struct ppages bitmap_pp;
    bitmap_t* root_bitmap;

    bitmap_nr_pages = root_pool->nr_pages / (8 * PAGE_SIZE) +
                           ((root_pool->nr_pages % (8 * PAGE_SIZE) != 0) ? 1 : 0);

    if (root_pool->nr_pages <= bitmap_nr_pages) {
        return false;
    }
    bitmap_base = (size_t) &__mem_vm_end;

    bitmap_pp = mem_ppages_get(bitmap_base, bitmap_nr_pages);
    root_bitmap = (bitmap_t*)bitmap_pp.base;
    root_pool->bitmap = root_bitmap;
    memset((void*)root_pool->bitmap, 0, bitmap_nr_pages * PAGE_SIZE);

    pageoff = NUM_PAGES(bitmap_pp.base - root_pool->base);

    bitmap_set_consecutive(root_pool->bitmap, pageoff, bitmap_pp.nr_pages);
    root_pool->free -= bitmap_pp.nr_pages;

    return true;
}

void mem_init() {
    static struct mem_region root_mem_region;
    struct page_pool *root_pool;

    root_mem_region.base = (size_t) &__mem_vm_begin;
    root_mem_region.size = (size_t) (&__mem_vm_end - &__mem_vm_begin);

    root_pool = &root_mem_region.page_pool;
    root_pool->base = ALIGN(root_mem_region.base, PAGE_SIZE);
    root_pool->nr_pages = root_mem_region.size / PAGE_SIZE;
    root_pool->free = root_pool->nr_pages;
    
    if (!root_pool_set_up_bitmap(&root_mem_region.page_pool)) {
        ERROR("ERROR.\n");
    }
    root_page_pool = root_pool;

    INFO("MEM INIT");
}

void *mem_alloc_page(size_t nr_pages, bool phys_aligned) {
    struct ppages ppages = mem_alloc_ppages(nr_pages, phys_aligned);

    return (void*)ppages.base;
}

bool pp_alloc(struct page_pool *pool, size_t nr_pages, bool aligned,
                     struct ppages *ppages) {
    bool ok = false;
    size_t start, curr, bit, next_aligned;

    ppages->nr_pages = 0;
    if (nr_pages == 0) {
        return true;
    }

    spin_lock(&pool->lock);

    /**
     *  If we need a contigous segment aligned to its size, lets start
     * at an already aligned index.
     */
    start = aligned ? pool->base / PAGE_SIZE % nr_pages : 0;
    curr = pool->last + ((pool->last + start) % nr_pages);

    /**
     * Lets make two searches:
     *  - one starting from the last known free index.
     *  - in case this does not work, start from index 0.
     */
    for (size_t i = 0; i < 2 && !ok; i++) {
        while (pool->free != 0) {
            bit = bitmap_find_consec(pool->bitmap, pool->nr_pages, curr, nr_pages, false);

            if (bit < 0) {
                /**
                 * No num_page page sement was found. If this is the first 
                 * iteration set position to 0 to start next search from index 0.
                 */
                next_aligned = (nr_pages - ((pool->base / PAGE_SIZE) % nr_pages)) % nr_pages;
                curr = aligned ? next_aligned : 0;

                break;
            } else if (aligned && (((bit + start) % nr_pages) != 0)) {
                /**
                 *  If we're looking for an aligned segment and the found
                 * contigous segment is not aligned, start the search again
                 * from the last aligned index
                 */
                curr = bit + ((bit + start) % nr_pages);
            } else {
                /**
                 * We've found our pages. Fill output argument info, mark
                 * them as allocated, and update page pool bookkeeping.
                 */
                ppages->base = pool->base + (bit * PAGE_SIZE);
                ppages->nr_pages = nr_pages;
                bitmap_set_consecutive(pool->bitmap, bit, nr_pages);
                pool->free -= nr_pages;
                pool->last = bit + nr_pages;
                ok = true;

                break;
            }
        }
    }
    spin_unlock(&pool->lock);

    return ok;
}

struct ppages mem_alloc_ppages(size_t nr_pages, bool aligned) {
    struct ppages pages = {.nr_pages = 0};

    if (!pp_alloc(root_page_pool, nr_pages, aligned, &pages)) {
        ERROR("not enough ppages");
    }

    return pages;
}
