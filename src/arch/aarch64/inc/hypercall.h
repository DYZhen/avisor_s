#ifndef HYPERCALL_H
#define HYPERCALL_H

typedef enum {
    // Halt
    HYPERCALL_ISS_HALT = 0,
    // Snapshot
    HYPERCALL_ISS_CHECKPOINT_SNAPSHOT,
    HYPERCALL_ISS_RESTORE_SNAPSHOT,
    // RQI
    HYPERCALL_ISS_RQ_OPEN,
    HYPERCALL_ISS_RQ_CLOSE,
    HYPERCALL_ISS_RQ_ATTACH,
    HYPERCALL_ISS_RQ_DETACH,
    // SCHE
    HYPERCALL_ISS_SCHE_START,
    HYPERCALL_ISS_IO_START,
} HYPERCALL_TYPE;

typedef void (*hypercall_handler_t)(unsigned long, unsigned long, unsigned long, unsigned long);

void hypercall_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);

#endif