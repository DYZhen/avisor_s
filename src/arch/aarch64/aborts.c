#include "aborts.h"
#include "util.h"
#include "vm.h"
#include "sysregs.h"
#include "hypercall.h"
#include "emul.h"

int cnt = 0;

static inline void addr_write(size_t addr, size_t width, uint64_t val) {
    switch (width) {
        case 1:
            *((uint8_t*)addr) = val;
            break;
        case 2:
            *((uint16_t*)addr) = val;
            break;
        case 4:
            *((uint32_t*)addr) = val;
            break;
        case 8:
            *((uint64_t*)addr) = val;
            break;
        default:
            ERROR("unknown addr write size");
    }
}

static inline unsigned long addr_read(size_t addr, size_t width, size_t sign_ext) {
    unsigned long val = 0;

    switch (width) {
        case 1:
            val = sign_ext ? *((int8_t*)addr)
                                 : *((uint8_t*)addr);
            break;
        case 2:
            val = sign_ext ? *((int16_t*)addr)
                                 : *((uint16_t*)addr);
            break;
        case 4:
            val = sign_ext ? *((int32_t*)addr)
                                 : *((uint32_t*)addr);
            break;
        case 8:
            val = *((uint64_t*)addr);
            break;
        default:
            ERROR("unknown addr read size");
    }

    return val;
}

void aborts_data_lower(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec) {
    unsigned long DSFC;
    size_t addr, width, write, reg, sign_ext;
    size_t reg_val, val, pc;
    unsigned long pc_step;
    struct emul_access emul;
    emul_handler_t handler = NULL;

    if (!(iss & ESR_ISS_DA_ISV_BIT) || (iss & ESR_ISS_DA_FnV_BIT)) {
        INFO("%d %d %d", iss, ESR_ISS_DA_ISV_BIT, ESR_ISS_DA_FnV_BIT);
        ERROR("no information to handle data abort (0x%x)", far);
    }

    DSFC = bit64_extract(iss, ESR_ISS_DA_DSFC_OFF, ESR_ISS_DA_DSFC_LEN) & (0xf << 2);

    if (DSFC != ESR_ISS_DA_DSFC_TRNSLT && DSFC != ESR_ISS_DA_DSFC_PERMIS) {
        ERROR("data abort is not translation fault - cant deal with it");
    }

    addr = far;
    handler = vm_emul_get_mem(cpu()->vcpu->vm, addr);
    if (handler == NULL) {
        width = (1 << bit64_extract(iss, ESR_ISS_DA_SAS_OFF, ESR_ISS_DA_SAS_LEN));
        write = iss & ESR_ISS_DA_WnR_BIT ? true : false;
        reg = bit64_extract(iss, ESR_ISS_DA_SRT_OFF, ESR_ISS_DA_SRT_LEN);
        // reg_width = 4 + (4 * bit64_extract(iss, ESR_ISS_DA_SF_OFF, ESR_ISS_DA_SF_LEN));
        sign_ext = bit64_extract(iss, ESR_ISS_DA_SSE_OFF, ESR_ISS_DA_SSE_LEN);

        // TODO: uart handler
        if (write) {
            reg_val = vcpu_readreg(cpu()->vcpu, reg);
            val = reg_val & BIT_MASK(0, width * 8);
            addr_write(addr, width, val);
        } else {
            vcpu_writereg(cpu()->vcpu, reg, addr_read(addr, width, sign_ext));
        }
    } else {
        emul.addr = addr;
        emul.width = (1 << bit64_extract(iss, ESR_ISS_DA_SAS_OFF, ESR_ISS_DA_SAS_LEN));
        emul.write = iss & ESR_ISS_DA_WnR_BIT ? true : false;
        emul.reg = bit64_extract(iss, ESR_ISS_DA_SRT_OFF, ESR_ISS_DA_SRT_LEN);
        emul.sign_ext = bit64_extract(iss, ESR_ISS_DA_SSE_OFF, ESR_ISS_DA_SSE_LEN);
        handler(&emul);
    }
    
    pc_step = 2 + (2 * il);
    pc = vcpu_readpc(cpu()->vcpu);
    vcpu_writepc(cpu()->vcpu, pc + pc_step);
}

void aborts_sync_hanlder() {
    uint64_t hsr = sysreg_esr_el2_read();
    uint64_t ec = bit64_extract(hsr, ESR_EC_OFF, ESR_EC_LEN);
    uint64_t iss = bit64_extract(hsr, ESR_ISS_OFF, ESR_ISS_LEN);
    uint64_t il = bit64_extract(hsr, ESR_IL_OFF, ESR_IL_LEN);
    unsigned long far = sysreg_far_el2_read();
    uint64_t arg0, arg1, arg2;

    if (ec == ESR_EC_DALEL) {
        aborts_data_lower(iss, far, il, ec);
    }

    if (ec == ESR_EC_HVC64) {
        arg0 = vcpu_readreg(cpu()->vcpu, 0);
        arg1 = vcpu_readreg(cpu()->vcpu, 1);
        arg2 = vcpu_readreg(cpu()->vcpu, 2);
        
        hypercall_handler(iss, arg0, arg1, arg2);
    }
}