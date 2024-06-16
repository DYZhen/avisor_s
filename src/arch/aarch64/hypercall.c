#include "hypercall.h"
#include "util.h"
#include "lcm.h"
#include "rq.h"

hypercall_handler_t hypercall_handlers[9] = {[HYPERCALL_ISS_HALT]                = guest_halt_hanlder,
                                             [HYPERCALL_ISS_CHECKPOINT_SNAPSHOT] = checkpoint_snapshot_hanlder,
                                             [HYPERCALL_ISS_RESTORE_SNAPSHOT]    = restore_snapshot_hanlder,
                                             [HYPERCALL_ISS_RQ_OPEN]             = rq_open_hanlder,
                                             [HYPERCALL_ISS_RQ_CLOSE]            = rq_close_hanlder,
                                             [HYPERCALL_ISS_RQ_ATTACH]           = rq_attach_hanlder,
                                             [HYPERCALL_ISS_RQ_DETACH]           = rq_detach_hanlder,
                                             [HYPERCALL_ISS_SCHE_START]          = sched_start_handler,
                                             [HYPERCALL_ISS_IO_START]            = sched_io_handler,
                                            };

void hypercall_handler(unsigned long iss, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    hypercall_handler_t hanlder = hypercall_handlers[iss];

    if (hanlder) { 
//        INFO("iss = %ld", iss);
//        INFO("arg0 = %ld", arg0);
//        INFO("arg1 = %ld", arg1);
//        INFO("arg2 = %ld", arg2);
        hanlder(iss, arg0, arg1, arg2);
    } else {
        ERROR("no handler for hypercall iss = 0x%x", iss);
    }

}