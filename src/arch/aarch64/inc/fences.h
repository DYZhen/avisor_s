#ifndef FENCES_H
#define FENCES_H

#include "util.h"

#define DMB(shdmn) asm volatile("dmb " XSTR(shdmn) "\n\t" ::: "memory")

#define DSB(shdmn) asm volatile("dsb " XSTR(shdmn) "\n\t" ::: "memory")

#define ISB() asm volatile("isb\n\t" ::: "memory")

static inline void fence_ord_write() {
    DMB(ishst);
}

static inline void fence_ord_read() {
    DMB(ishld);
}

static inline void fence_ord() {
    DMB(ish);
}

static inline void fence_sync_write() {
    DSB(ishst);
}

static inline void fence_sync_read() {
    DSB(ishld);
}

static inline void fence_sync() {
    DSB(ish);
}

#endif
