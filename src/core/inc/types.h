#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

typedef size_t paddr_t;
typedef size_t vaddr_t;

typedef signed long ssize_t;

typedef unsigned long cpuid_t;
typedef unsigned long devid_t;
typedef unsigned long irqid_t;
typedef unsigned long asid_t;
typedef unsigned long cpuid_t;
typedef unsigned long vcpuid_t;
typedef unsigned long vmid_t;

typedef uint64_t ssid_t;

typedef unsigned deviceid_t;
typedef deviceid_t streamid_t;

typedef unsigned long cpumap_t;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


#define REG32   uint32_t    // 32 bit register
#define REG64   uint64_t    // 64 bit register
#define SREG64  uint64_t    // special registers

#define MAX_VA  ((vaddr_t)-1)
#define INVALID_VA MAX_VA
#define INVALID_CPUID   ((cpuid_t)-1)

typedef enum type { AS_HYP = 0, AS_VM, AS_HYP_CPY } as_type_t;

#endif