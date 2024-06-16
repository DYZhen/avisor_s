#include "util.h"
#include "vmm.h"
#include "vm.h"
#include "cpu.h"
#include "interrupts.h"

int main(cpuid_t id) {
     cpu_init(id);

     if (id == CPU_MASTER) {
          INFO("------------avisor started------------");
          INFO("Exception level: %d", sysreg_CurrentEL_read() >> 2);
          mem_init();
     }
     
     cpu_sync_barrier(&cpu_glb_sync);

     interrupts_init();

     vmm_init();

     while(1);

     return 0;
}
