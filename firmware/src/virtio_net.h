#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include "lwip/netif.h"

/* Initialize VirtIO network interface */
struct netif* virtio_net_init(void);

/* Poll for network activity (call from main loop) */
void virtio_net_poll(void);

/* Interrupt handler (called from trap handler) */
void virtio_net_irq_handler(void);

#endif /* VIRTIO_NET_H */
