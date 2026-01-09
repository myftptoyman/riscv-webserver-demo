#include "timer.h"
#include "platform.h"

/* System time in milliseconds */
static uint32_t sys_now_ms = 0;
static uint64_t last_mtime = 0;

void timer_init(void) {
    last_mtime = MMIO_READ64(CLINT_MTIME);
}

/* Get current time in milliseconds */
uint32_t sys_now(void) {
    uint64_t current = MMIO_READ64(CLINT_MTIME);
    uint64_t delta = current - last_mtime;

    /* Convert ticks to milliseconds */
    uint32_t ms_delta = (uint32_t)((delta * 1000) / TIMER_FREQ);
    if (ms_delta > 0) {
        sys_now_ms += ms_delta;
        last_mtime = current;
    }

    return sys_now_ms;
}

void timer_irq_handler(void) {
    /* Not used in polling mode */
}
