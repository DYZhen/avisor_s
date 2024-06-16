#ifndef RQ_H
#define RQ_H

#include "util.h"

struct rq_config_vm {
    vaddr_t vbase;
    size_t rq_size;
};

struct rq_vm {
    vaddr_t vbase;
    paddr_t pbase;
    size_t rq_size;
};

// Hypercall Hanlder
void rq_open_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);
void rq_close_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);
void rq_attach_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);
void rq_detach_hanlder(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2);

void vm_rq_init(struct vm* vm, const struct vm_config* config);

#endif