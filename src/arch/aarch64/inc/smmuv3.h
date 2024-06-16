#ifndef SMMUV3_H
#define SMMUV3_H

#include "util.h"

void arm_smmuv3_init();
int arm_smmuv3_vm_dev_init(vmid_t vmid, streamid_t sid);

#endif