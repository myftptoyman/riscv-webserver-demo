#include "platform.h"
#include "plic.h"
#include "console.h"

/* Forward declarations for device handlers */
extern void virtio_net_irq_handler(void);

/* CSR read helpers */
static inline uint64_t read_mcause(void) {
    uint64_t val;
    __asm__ volatile("csrr %0, mcause" : "=r"(val));
    return val;
}

static inline uint64_t read_mepc(void) {
    uint64_t val;
    __asm__ volatile("csrr %0, mepc" : "=r"(val));
    return val;
}

static inline uint64_t read_mtval(void) {
    uint64_t val;
    __asm__ volatile("csrr %0, mtval" : "=r"(val));
    return val;
}

/* Trap handler called from assembly */
void trap_handler(void) {
    uint64_t mcause = read_mcause();
    uint64_t mepc = read_mepc();

    int is_interrupt = (mcause >> 63) & 1;
    uint64_t code = mcause & 0x7FFFFFFFFFFFFFFF;

    if (is_interrupt) {
        /* External interrupt */
        if (code == 11) { /* Machine external interrupt */
            uint32_t irq = plic_claim();
            if (irq != 0) {
                if (irq == VIRTIO_FIFO_INT_ID) {
                    virtio_net_irq_handler();
                }
                plic_complete(irq);
            }
        }
    } else {
        /* Exception */
        console_printf("Exception: mcause=0x%lx mepc=0x%lx mtval=0x%lx\n",
                      mcause, mepc, read_mtval());
        while (1);
    }
}
