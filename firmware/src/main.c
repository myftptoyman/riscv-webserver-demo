/*
 * main.c - Bare-metal lwIP web server for RISC-V
 */

#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "lwip/ip4_addr.h"

#include "virtio_net.h"
#include "timer.h"
#include "heap.h"
#include "console.h"
#include "plic.h"

#include <string.h>

/* Static HTML page */
static const char html_page[] =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "  <title>RISC-V lwIP Web Server</title>\n"
    "  <style>\n"
    "    body { font-family: sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }\n"
    "    h1 { color: #333; }\n"
    "    .info { background: #f0f0f0; padding: 15px; border-radius: 5px; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <h1>Hello from RISC-V!</h1>\n"
    "  <div class=\"info\">\n"
    "    <p>This page is served by a bare-metal web server running on:</p>\n"
    "    <ul>\n"
    "      <li><strong>Platform:</strong> Spike RISC-V Simulator</li>\n"
    "      <li><strong>TCP/IP Stack:</strong> lwIP</li>\n"
    "      <li><strong>Network:</strong> VirtIO FIFO + SLIRP</li>\n"
    "    </ul>\n"
    "  </div>\n"
    "  <p>The entire system runs without an operating system!</p>\n"
    "</body>\n"
    "</html>\n";

static const char http_ok[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n"
    "Content-Length: ";

static const char http_404[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "\r\n"
    "404 Not Found\n";

/* HTTP connection state */
struct http_state {
    int sent_headers;
    int sent_body;
};

/* Format a number to string */
static int int_to_str(char *buf, int val) {
    char tmp[12];
    int i = 0, j = 0;

    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }

    while (i > 0) {
        buf[j++] = tmp[--i];
    }

    return j;
}

/* TCP receive callback */
static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    struct http_state *hs = (struct http_state*)arg;

    if (p == NULL) {
        /* Connection closed by remote */
        tcp_close(pcb);
        if (hs) free(hs);
        return ERR_OK;
    }

    /* Acknowledge received data */
    tcp_recved(pcb, p->tot_len);

    /* Check if this is a GET request */
    char *data = (char*)p->payload;
    int is_get = (p->len >= 3 && data[0] == 'G' && data[1] == 'E' && data[2] == 'T');

    pbuf_free(p);

    if (!hs->sent_headers && is_get) {
        /* Build HTTP response */
        char header[256];
        int len = 0;

        /* Copy status line and headers */
        memcpy(header + len, http_ok, sizeof(http_ok) - 1);
        len += sizeof(http_ok) - 1;

        /* Add content length */
        len += int_to_str(header + len, sizeof(html_page) - 1);
        header[len++] = '\r';
        header[len++] = '\n';
        header[len++] = '\r';
        header[len++] = '\n';

        /* Send headers */
        tcp_write(pcb, header, len, TCP_WRITE_FLAG_COPY);
        hs->sent_headers = 1;

        /* Send body */
        tcp_write(pcb, html_page, sizeof(html_page) - 1, TCP_WRITE_FLAG_COPY);
        hs->sent_body = 1;

        tcp_output(pcb);

        /* Close after sending */
        tcp_close(pcb);
    } else if (!is_get) {
        /* Send 404 for non-GET requests */
        tcp_write(pcb, http_404, sizeof(http_404) - 1, TCP_WRITE_FLAG_COPY);
        tcp_output(pcb);
        tcp_close(pcb);
    }

    return ERR_OK;
}

/* TCP error callback */
static void http_err(void *arg, err_t err) {
    struct http_state *hs = (struct http_state*)arg;
    (void)err;
    if (hs) free(hs);
}

/* TCP accept callback */
static err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    (void)err;

    struct http_state *hs = (struct http_state*)calloc(1, sizeof(struct http_state));
    if (hs == NULL) {
        tcp_abort(newpcb);
        return ERR_MEM;
    }

    tcp_arg(newpcb, hs);
    tcp_recv(newpcb, http_recv);
    tcp_err(newpcb, http_err);

    console_printf("HTTP: new connection\n");
    return ERR_OK;
}

/* Initialize HTTP server */
static void http_server_init(void) {
    struct tcp_pcb *pcb;

    pcb = tcp_new();
    if (pcb == NULL) {
        console_printf("Failed to create TCP PCB\n");
        return;
    }

    err_t err = tcp_bind(pcb, IP_ADDR_ANY, 80);
    if (err != ERR_OK) {
        console_printf("Failed to bind to port 80: %d\n", err);
        return;
    }

    pcb = tcp_listen(pcb);
    if (pcb == NULL) {
        console_printf("Failed to listen\n");
        return;
    }

    tcp_accept(pcb, http_accept);
    console_printf("HTTP server listening on port 80\n");
}

/* HTIF exit */
extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;

static void htif_exit(int code) {
    while (tohost) {
        fromhost = 0;
    }
    tohost = (code << 1) | 1;
    while (1);
}

/* Main entry point */
int main(void) {
    console_init();
    console_printf("\n");
    console_printf("========================================\n");
    console_printf("   RISC-V lwIP Web Server\n");
    console_printf("========================================\n");
    console_printf("\n");

    /* Initialize heap */
    heap_init();
    console_printf("[OK] Heap initialized\n");

    /* Initialize timer */
    timer_init();
    console_printf("[OK] Timer initialized\n");

    /* Initialize PLIC */
    plic_init();
    console_printf("[OK] PLIC initialized\n");

    /* Initialize lwIP */
    lwip_init();
    console_printf("[OK] lwIP initialized\n");

    /* Initialize network interface */
    struct netif *netif = virtio_net_init();
    if (netif == NULL) {
        console_printf("[FAIL] Network init failed\n");
        htif_exit(1);
    }
    console_printf("[OK] Network interface ready\n");

    /* Start HTTP server */
    http_server_init();

    console_printf("\n");
    console_printf("System ready! Access http://localhost:8080 from host.\n");
    console_printf("Entering main loop...\n");
    console_printf("\n");

    /* Main loop */
    uint32_t last_time = 0;
    while (1) {
        /* Poll for network activity */
        virtio_net_poll();

        /* Handle lwIP timers */
        sys_check_timeouts();

        /* Periodic status (every 10 seconds) */
        uint32_t now = sys_now();
        if (now - last_time >= 10000) {
            console_printf("Uptime: %u seconds\n", now / 1000);
            last_time = now;
        }
    }

    return 0;
}
