#ifndef BIT_H
#define BIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * The extra shift is because both arm and riscv logical shift instructions
 * support a maximum of machine word length minus one bit shits. This covers
 * the corner case of runtime full machine word length masks with the cost of
 * an extra shift instruction. For static masks, there should be no extra costs.
 */
#define BIT32_MASK(OFF, LEN) ((((UINT32_C(1)<<((LEN)-1))<<1)-1)<<(OFF))
#define BIT64_MASK(OFF, LEN) ((((UINT64_C(1)<<((LEN)-1))<<1)-1)<<(OFF))
#define BIT_MASK(OFF, LEN) (((((1UL)<<((LEN)-1))<<1)-1)<<(OFF))

/* create 64-bit mask with all bits in [last:first] set */
#define BIT_MASK_LR(last, first) \
	((0xffffffffffffffffULL >> (64 - ((last) + 1 - (first)))) << (first))

#define max(n1, n2) (((n1) > (n2)) ? (n1) : (n2))
#define min(n1, n2) (((n1) < (n2)) ? (n1) : (n2))

#define BIT_OPS_GEN(PRE, TYPE, LIT, MASK) \
    static inline TYPE PRE ## _get(TYPE word, size_t off)\
    {\
        return word & ((LIT) << off);\
    }\
    static inline TYPE PRE ## _set(TYPE word, size_t off)\
    {\
        return word |= ((LIT) << off);\
    }\
    static inline TYPE PRE ## _clear(TYPE word, size_t off)\
    {\
        return word &= ~((LIT) << off);\
    }\
    static inline TYPE PRE ## _extract(TYPE word, size_t off, size_t len)\
    {\
        return (word >> off) & MASK(0, len);\
    }\
    static inline TYPE PRE ## _insert(TYPE word, TYPE val, size_t off,\
                                    size_t len)\
    {\
        return (~MASK(off, len) & word) | ((MASK(0, len) & val) << off);\
    }\
    static inline size_t PRE ## _ffs(TYPE word)\
    {\
        size_t pos = (size_t)0;\
        TYPE mask = (LIT);\
        while (mask != 0U) {\
            if ((mask & word) != 0U) {\
                break;\
            }\
            mask <<= 1U;\
            pos++;\
        }\
        return (mask != 0U) ? pos : (size_t)-1;\
    }\
    static inline size_t PRE ## _count(TYPE word)\
    {\
        size_t count = 0;\
        TYPE mask = (LIT);\
        while (mask != 0U) {\
            if ((mask & word) != 0U) {\
                count += 1;\
            }\
            mask <<= 1U;\
        }\
        return count;\
    }

BIT_OPS_GEN(bit32, uint32_t, UINT32_C(1), BIT32_MASK);
BIT_OPS_GEN(bit64, uint64_t, UINT64_C(1), BIT64_MASK);
BIT_OPS_GEN(bit, unsigned long, (1UL), BIT_MASK);

#endif
