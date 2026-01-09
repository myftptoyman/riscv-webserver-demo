#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/* Memory-mapped device base addresses (from Spike) */
#define CLINT_BASE          0x02000000
#define CLINT_MTIME         (CLINT_BASE + 0xBFF8)
#define CLINT_MTIMECMP      (CLINT_BASE + 0x4000)

#define PLIC_BASE           0x0C000000
#define PLIC_PRIORITY       (PLIC_BASE + 0x0000)
#define PLIC_PENDING        (PLIC_BASE + 0x1000)
#define PLIC_MENABLE        (PLIC_BASE + 0x2000)
#define PLIC_MTHRESHOLD     (PLIC_BASE + 0x200000)
#define PLIC_MCLAIM         (PLIC_BASE + 0x200004)

#define NS16550_BASE        0x10000000
#define NS16550_THR         (NS16550_BASE + 0)
#define NS16550_RBR         (NS16550_BASE + 0)
#define NS16550_LSR         (NS16550_BASE + 5)

#define VIRTIO_FIFO_BASE    0x10001000
#define VIRTIO_FIFO_INT_ID  2

/* Timer frequency (10 MHz in Spike) */
#define TIMER_FREQ          10000000

/* MMIO helpers */
#define MMIO_READ8(addr)    (*(volatile uint8_t*)(addr))
#define MMIO_READ32(addr)   (*(volatile uint32_t*)(addr))
#define MMIO_READ64(addr)   (*(volatile uint64_t*)(addr))
#define MMIO_WRITE8(addr, val)  (*(volatile uint8_t*)(addr) = (val))
#define MMIO_WRITE32(addr, val) (*(volatile uint32_t*)(addr) = (val))
#define MMIO_WRITE64(addr, val) (*(volatile uint64_t*)(addr) = (val))

#endif /* PLATFORM_H */
