#ifndef VGICV2_H
#define VGICV2_H

#include "vgic.h"
#include "vm.h"

static inline bool vgic_int_vcpu_is_target(struct vcpu *vcpu, struct vgic_int *interrupt) {
    bool priv = gic_is_priv(interrupt->id);
    bool target = interrupt->targets & (1 << vcpu->p_id);

    return priv || target;
}

#endif