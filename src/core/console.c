#include "console.h"
#include "uart.h"
#include "spinlock.h"
 
spinlock_t lock = SPINLOCK_INITVAL;

void console_init() {

}

void console_write(char const* const str) {
    spin_lock(&lock);
    uart_print_string(str);
    spin_unlock(&lock);
}