#include "mmu.h"
#include "mem.h"
#include "fences.h"
#include "config.h"
#include "string.h"
#include "util.h"
#include "tlb.h"

static inline bool pte_allocable(struct addr_space *as, pte_t *pte, size_t lvl,
                                size_t left, vaddr_t addr) {
    return (lvl == (as->pt.dscr->lvls - 1)) ||
           (pt_lvl_terminal(&as->pt, lvl) && !pte_valid(pte) &&
            (pt_lvlsize(&as->pt, lvl) <= (left * PAGE_SIZE)) &&
            ((addr % pt_lvlsize(&as->pt, lvl)) == 0));
}

static inline pte_t *mem_alloc_pt(struct addr_space *as, pte_t *parent, size_t lvl,
                                  vaddr_t addr) {
    /* Must have lock on as and va section to call */
    pte_t pte_dflt_val;
    pte_t *temp_pt;
    size_t ptsize = NUM_PAGES(pt_size(&as->pt, lvl + 1));
    struct ppages ppage = mem_alloc_ppages(ptsize, ptsize > 1 ? true : false);

    if (ppage.nr_pages == 0) {
        return NULL;
    }

    pte_dflt_val = PTE_INVALID | (*parent & PTE_RSW_MSK);
    pte_set(parent, ppage.base, PTE_TABLE, PTE_HYP_FLAGS);
    fence_sync_write();
    temp_pt = pt_get(&as->pt, lvl + 1, addr);

    for (size_t i = 0; i < pt_nentries(&as->pt, lvl + 1); i++) {
        temp_pt[i] = pte_dflt_val;
    }

    return temp_pt;
}

static void mem_free_ppages(struct ppages *ppages) {
    // list_foreach(page_pool_list, struct page_pool, pool)
    // {
    //     spin_lock(&pool->lock);
    //     if (in_range(ppages->base, pool->base, pool->size * PAGE_SIZE)) {
    //         size_t index = (ppages->base - pool->base) / PAGE_SIZE;
    //         if (!all_clrs(ppages->colors)) {
    //             for (size_t i = 0; i < ppages->num_pages; i++) {
    //                 index = pp_next_clr(pool->base, index, ppages->colors);
    //                 bitmap_clear(pool->bitmap, index++);
    //             }
    //         } else {
    //             bitmap_clear_consecutive(pool->bitmap, index, ppages->num_pages);
    //         }
    //     }
    //     spin_unlock(&pool->lock);
    // }
    size_t index;
    struct page_pool *pool = root_page_pool;

    spin_lock(&pool->lock);
    if (in_range(ppages->base, pool->base, pool->nr_pages * PAGE_SIZE)) {
        index = (ppages->base - pool->base) / PAGE_SIZE;
        bitmap_clear_consecutive(pool->bitmap, index, ppages->nr_pages);
    }
    spin_lock(&pool->lock);
}

static inline pte_type_t pt_page_type(struct page_table* pt, size_t lvl) {
    return (lvl == pt->dscr->lvls - 1) ? PTE_PAGE : PTE_SUPERPAGE;
}

static void mem_expand_pte(struct addr_space *as, vaddr_t va, size_t lvl) {
    /* Must have lock on as and va section to call */
    pte_t *pte;
    pte_t pte_val;
    bool rsv, vld;
    paddr_t paddr;
    size_t entry, nentries, lvlsz;
    pte_type_t type;
    pte_flags_t flags;
    
    if (as->pt.dscr->lvls - 1 <= lvl) {
        /* no more levels to expand */
        return;
    }

    pte = pt_get_pte(&as->pt, lvl, va);

    /**
     * only can expand if the pte exists and it isnt pointing to
     * a next level table already.
     */
    if (pte != NULL && !pte_table(&as->pt, pte, lvl)) {
        pte_val = *pte;  // save the original pte
        rsv = pte_check_rsw(pte, PTE_RSW_RSRV);
        vld = pte_valid(pte);
        pte = mem_alloc_pt(as, pte, lvl, va);

        if (vld || rsv) {
            /**
             *  If this was valid before and it wasn't a table, it must
             * have been a superpage, so fill the new expanded table to
             * have the same mappings;
             */

            /**
             * Invalidate the old TLB entries with superpage entries.
             * This means that from now on to the end of the function,
             * the original spaced mapped by the entry will be unmaped.
             * Therefore this function cannot be call on the entry mapping
             * hypervisor code or data used in it (including stack).
             */
            // tlb_inv_va(&cpu()->as, va);

            /**
             *  Now traverse the new next level page table to replicate the
             * original mapping.
             */

            lvl++;
            paddr = pte_addr(&pte_val);
            entry = pt_getpteindex(&as->pt, pte, lvl);
            nentries = pt_nentries(&as->pt, lvl);
            lvlsz = pt_lvlsize(&as->pt, lvl);
            type = pt_page_type(&as->pt, lvl);
            flags = (as->type == AS_HYP ? PTE_HYP_FLAGS : PTE_VM_FLAGS);

            while (entry < nentries) {
                if (vld) {
                    pte_set(pte, paddr, type, flags);
                } else if (rsv) {
                    pte_set_rsw(pte, PTE_RSW_RSRV);
                }

                pte++;
                entry++;
                paddr += lvlsz;
            }

            fence_sync_write();
        }
    }
}

static void mem_inflate_pt(struct addr_space *as, vaddr_t va, size_t length) {
    /* Must have lock on as and va section to call */

    /**
     * For each level in the pt, expand each entry in the specified range
     * as a next level page table.
     */
    for (size_t lvl = 0; lvl < as->pt.dscr->lvls - 1; lvl++) {
        vaddr_t vaddr = va;
        size_t lvlsz = pt_lvlsize(&as->pt, lvl);
        while (vaddr < (va + length)) {
            mem_expand_pte(as, vaddr, lvl);
            vaddr += lvlsz;
        }
    }
}


void as_init(struct addr_space *as, enum type type, asid_t asid, 
            pte_t *root_pt) {
    size_t n;

    as->type = type;
    as->pt.dscr =
        type == AS_HYP || type == AS_HYP_CPY ? hyp_pt_dscr : vm_pt_dscr;
    as->lock = SPINLOCK_INITVAL;
    as->asid = asid;

    if (root_pt == NULL) {
        n = NUM_PAGES(pt_size(&as->pt, 0));
        root_pt = (pte_t*) mem_alloc_page(n, true);
        memset((void*)root_pt, 0, n * PAGE_SIZE);
    }
    as->pt.root = root_pt;

    as_arch_init(as);
}

vaddr_t mem_alloc_vpage(struct addr_space *as, vaddr_t at, size_t n) {
    size_t lvl = 0;
    size_t entry = 0;
    size_t nentries = 0;
    size_t lvlsze = 0;
    size_t count = 0;
    size_t full_as;
    vaddr_t addr = INVALID_VA;
    vaddr_t vpage = INVALID_VA;
    vaddr_t top = MAX_VA;
    pte_t *pte = NULL;
    bool failed = false;

    
    if (at != INVALID_VA) {
        addr = at;
    } else {
        addr = 0x0;
    }
    top = MAX_VA;
    
    if (addr > top || !IS_ALIGNED(addr, PAGE_SIZE)) {
        return INVALID_VA;
    }

    spin_lock(&as->lock);

    while (count < n && !failed) {
        full_as = (addr == 0) && (top == MAX_VA);
        if (!full_as && (((top + 1 - addr) / PAGE_SIZE) < n)) {
            vpage = INVALID_VA;
            failed = true;
            break;
        }

        pte = pt_get_pte(&as->pt, lvl, addr);
        entry = pt_getpteindex(&as->pt, pte, lvl);
        nentries = pt_nentries(&as->pt, lvl);
        lvlsze = pt_lvlsize(&as->pt, lvl);

        while ((entry < nentries) && (count < n) && !failed) {
            if (pte_check_rsw(pte, PTE_RSW_RSRV) ||
                (pte_valid(pte) && !pte_table(&as->pt, pte, lvl))) {
                count = 0;
                vpage = INVALID_VA;
                if (at != INVALID_VA) {
                    failed = true;
                    break;
                }
            } else if (!pte_valid(pte)) {
                if (pte_allocable(as, pte, lvl, n - count, addr)) {
                    if (count == 0) vpage = addr;
                    count += (lvlsze / PAGE_SIZE);
                } else {
                    if (mem_alloc_pt(as, pte, lvl, addr) == NULL) {
                        ERROR("Failed to alloc page table.\n");
                    }
                }
            }

            if (pte_table(&as->pt, pte, lvl)) {
                lvl++;
                break;
            } else {
                pte++;
                addr += lvlsze;
                if (++entry >= nentries) {
                    lvl = 0;
                    break;
                }
            }
        }
    }

    if (vpage != INVALID_VA && !failed) {
        count = 0;
        addr = vpage;
        lvl = 0;
        while (count < n) {
            for (lvl = 0; lvl < as->pt.dscr->lvls; lvl++) {
                pte = pt_get_pte(&as->pt, lvl, addr);
                if (!pte_valid(pte)) break;
            }
            pte_set_rsw(pte, PTE_RSW_RSRV);
            addr += pt_lvlsize(&as->pt, lvl);
            count += pt_lvlsize(&as->pt, lvl) / PAGE_SIZE;
        }
    }

    spin_unlock(&as->lock);
    
    return vpage;
}

bool mem_map(struct addr_space *as, vaddr_t va, struct ppages *ppages,
            size_t nr_pages, mem_flags_t flags) {
    pte_t *pte = NULL;
    vaddr_t vaddr = va & ~(PAGE_SIZE - 1);

    spin_lock(&as->lock);

    /**
     * TODO check if entry is reserved. Unrolling mapping if something
     * goes wrong.
     */

    struct ppages temp_ppages;
    if (ppages == NULL) {
        temp_ppages = mem_alloc_ppages(nr_pages, false);
        if (temp_ppages.nr_pages < nr_pages) {
            ERROR("failed to alloc colored physical pages");
        }
        ppages = &temp_ppages;
    }

    mem_inflate_pt(as, vaddr, nr_pages * PAGE_SIZE);
    for (size_t i = 0; i < ppages->nr_pages; i++) {
        pte = pt_get_pte(&as->pt, as->pt.dscr->lvls - 1, vaddr);
        paddr_t paddr = ppages->base + (i * PAGE_SIZE);
        pte_set(pte, paddr, PTE_PAGE, flags);
        vaddr += PAGE_SIZE;
    }

    fence_sync();
    spin_unlock(&as->lock);

    return true;
}

vaddr_t mem_alloc_map(struct addr_space* as, struct ppages *page, 
                        vaddr_t at, size_t nr_pages, mem_flags_t flags) {
    vaddr_t address = mem_alloc_vpage(as, at, nr_pages);

    if (address != INVALID_VA) {
        mem_map(as, address, page, nr_pages, flags);
    }

    return address;
}

vaddr_t mem_alloc_map_dev(struct addr_space* as, 
                             vaddr_t at, paddr_t pa, size_t nr_pages) {
    vaddr_t address = mem_alloc_vpage(as, at, nr_pages);
    
    if (address != INVALID_VA) {
        struct ppages pages = mem_ppages_get(pa, nr_pages);
        mem_flags_t flags = 
            as->type == AS_HYP ? PTE_HYP_DEV_FLAGS : PTE_VM_DEV_FLAGS;
        mem_map(as, address, &pages, nr_pages, flags);
    }

    return address;
}

void mem_unmap(struct addr_space* as, vaddr_t at, size_t num_pages, bool free_ppages) {
    vaddr_t vaddr = at;
    vaddr_t top = at + (num_pages * PAGE_SIZE);
    size_t lvl = 0;
    pte_t *pte = NULL;
    size_t lvlsz, entry, nentries;
    vaddr_t vpage_base;
    paddr_t paddr;
    struct ppages ppages;

    spin_lock(&as->lock);

    while (vaddr < top) {
        pte = pt_get_pte(&as->pt, lvl, vaddr);
        if (pte == NULL) {
            ERROR("invalid pte while freeing vpages");
        } else if (!pte_valid(pte)) {
            lvlsz = pt_lvlsize(&as->pt, lvl);
            vaddr += lvlsz;
        } else if (pte_table(&as->pt, pte, lvl)) {
            lvl++;
        } else {
            entry = pt_getpteindex(&as->pt, pte, lvl);
            nentries = pt_nentries(&as->pt, lvl);
            lvlsz = pt_lvlsize(&as->pt, lvl);

            while ((entry < nentries) && (vaddr < top)) {
                if (!pte_table(&as->pt, pte, lvl)) {
                    vpage_base = vaddr & ~(lvlsz - 1);

                    if (vaddr > vpage_base || top < (vpage_base + lvlsz)) {
                        mem_expand_pte(as, vaddr, lvl);
                        lvl++;
                        break;
                    }

                    if (free_ppages) {
                        paddr = pte_addr(pte);
                        ppages = mem_ppages_get(paddr, lvlsz / PAGE_SIZE);
                        mem_free_ppages(&ppages);
                    }

                    *pte = 0;
                    tlb_inv_va(&cpu()->vcpu->vm->as, vaddr);

                } else {
                    break;
                }
                pte++;
                entry++;
                vaddr += lvlsz;
            }

            if (entry == nentries) {
                lvl--;
            }

            /**
             * TODO: check if the current pt is now empty and if so,
             * free it too up to the root.
             */
        }
    }

    spin_unlock(&as->lock);
}