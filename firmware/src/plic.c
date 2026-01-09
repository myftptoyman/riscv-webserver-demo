#include "plic.h"
#include "platform.h"

void plic_init(void) {
    /* Set threshold to 0 (accept all priorities > 0) */
    MMIO_WRITE32(PLIC_MTHRESHOLD, 0);
}

void plic_enable(uint32_t irq) {
    /* Set priority */
    MMIO_WRITE32(PLIC_PRIORITY + irq * 4, 1);

    /* Enable interrupt in machine mode */
    uint32_t word = irq / 32;
    uint32_t bit = irq % 32;
    uint32_t current = MMIO_READ32(PLIC_MENABLE + word * 4);
    MMIO_WRITE32(PLIC_MENABLE + word * 4, current | (1 << bit));
}

void plic_disable(uint32_t irq) {
    uint32_t word = irq / 32;
    uint32_t bit = irq % 32;
    uint32_t current = MMIO_READ32(PLIC_MENABLE + word * 4);
    MMIO_WRITE32(PLIC_MENABLE + word * 4, current & ~(1 << bit));
}

uint32_t plic_claim(void) {
    return MMIO_READ32(PLIC_MCLAIM);
}

void plic_complete(uint32_t irq) {
    MMIO_WRITE32(PLIC_MCLAIM, irq);
}
