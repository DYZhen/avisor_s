#ifndef TIMER_H
#define TIMER_H

#include "util.h"

// Timer SBSA recommended INTID mappings
// EL1 Physical Timer: 30
// EL1 Virtual Timer: 27
// Non-secure EL2 Physical Timer: 26
// Non-secure EL2 Virtual Timer: 28
// EL3 Physical Timer: 29
// Secure EL2 Physical Timer: 20
// Secure EL2 Virtual Timer: 19
#define IRQ_TIMER (26)

void timer_init();
void timer_handler();

#endif