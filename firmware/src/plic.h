#ifndef PLIC_H
#define PLIC_H

#include <stdint.h>

void plic_init(void);
void plic_enable(uint32_t irq);
void plic_disable(uint32_t irq);
uint32_t plic_claim(void);
void plic_complete(uint32_t irq);

#endif /* PLIC_H */
