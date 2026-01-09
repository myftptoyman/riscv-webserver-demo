#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* Bare-metal, single-threaded - no OS */
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Memory configuration */
#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1
#define MEM_ALIGNMENT               8
#define MEM_SIZE                    (128 * 1024)   /* 128KB heap for lwIP */

/* Pool configuration */
#define MEMP_NUM_PBUF               32
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_PCB_LISTEN     4
#define MEMP_NUM_TCP_SEG            32
#define PBUF_POOL_SIZE              32
#define PBUF_POOL_BUFSIZE           1600

/* TCP configuration */
#define LWIP_TCP                    1
#define TCP_MSS                     1460
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_SND_QUEUELEN            16
#define TCP_QUEUE_OOSEQ             1
#define LWIP_TCP_KEEPALIVE          1

/* IP configuration */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ICMP                   1
#define LWIP_RAW                    0
#define LWIP_UDP                    1

/* DHCP - disabled, using static IP */
#define LWIP_DHCP                   0

/* ARP configuration */
#define LWIP_ARP                    1
#define ARP_TABLE_SIZE              10
#define ARP_QUEUEING                1
#define ETHARP_SUPPORT_STATIC_ENTRIES 1

/* Ethernet configuration */
#define LWIP_ETHERNET               1
#define ETH_PAD_SIZE                0

/* Callback-style API */
#define LWIP_CALLBACK_API           1

/* Debugging - disable for production */
#define LWIP_DEBUG                  0
#define LWIP_DBG_MIN_LEVEL          LWIP_DBG_LEVEL_ALL
#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF

/* Checksum configuration - compute in software */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_GEN_ICMP           1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1
#define CHECKSUM_CHECK_ICMP         1

/* Statistics - disable for size */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

/* Loopback */
#define LWIP_NETIF_LOOPBACK         0
#define LWIP_HAVE_LOOPIF            0

/* Let lwIP provide its own errno values (no system errno.h) */
#define LWIP_PROVIDE_ERRNO          1

#endif /* LWIPOPTS_H */
