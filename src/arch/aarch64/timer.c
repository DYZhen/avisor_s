#include "util.h"
#include "timer.h"
#include "sysregs.h"
#include "interrupts.h"

#define CNTV_CTL_ENABLE         (1 << 0)    /* Enables the timer */
#define CNTV_CTL_IMASK          (1 << 1)    /* Timer interrupt mask bit */
#define CNTV_CTL_ISTATUS        (1 << 2)    /* The status of the timer interrupt. This bit is read-only */

uint64_t TIMER_WAIT = 2000;
static uint64_t cntfrq = 0;

static inline void disable_cnthp(void) {
	uint32_t cntv_ctl;
	
	cntv_ctl = sysreg_cnthp_ctl_el2_read();
	cntv_ctl &= ~CNTV_CTL_ENABLE;
    sysreg_cnthp_ctl_el2_write(cntv_ctl);
}

static inline void enable_cnthp(void) {
	uint32_t cntv_ctl;
	
	cntv_ctl = sysreg_cnthp_ctl_el2_read();
	cntv_ctl |= CNTV_CTL_ENABLE;
	sysreg_cnthp_ctl_el2_write(cntv_ctl);
}

void set_el2_timer_sec(uint64_t _sec) {
	TIMER_WAIT = _sec * 1000;
}

void set_el2_timer_microsec(uint64_t _ms) {
	TIMER_WAIT = _ms;
}

void timer_handler(void) {
//    INFO("time handler CPU id = %d", cpu()->id);
    
	static int cnt = 0;
	uint64_t ticks, current_cnt;
	// uint32_t val;

	// Disable the timer
	disable_cnthp();
	// val = sysreg_cnthp_ctl_el2_read();

	ticks = TIMER_WAIT * cntfrq / 1000;
	
	current_cnt = sysreg_cntpct_el0_read();
    sysreg_cnthp_cval_el2_write(current_cnt + ticks);

	// INFO("do irq_timer next timestamp: %d", current_cnt + ticks);
	INFO("TIME IRQ %d", ++cnt);

	// Enable the timer
	enable_cnthp();
}


void timer_init(void) {
	// uint64_t current_cnt;

	// Disable the timer
	disable_cnthp();

    set_el2_timer_sec(1);
	cntfrq = sysreg_cntfrq_el0_read();
	INFO("EL2 timer frq = %d", cntfrq);
	// current_cnt = sysreg_cntpct_el0_read();

	enable_cnthp();
}