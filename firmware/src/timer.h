#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(void);
uint32_t sys_now(void);
void timer_irq_handler(void);

#endif /* TIMER_H */
