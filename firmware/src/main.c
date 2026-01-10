/*
 * main.c - Bare-metal lwIP web server for RISC-V
 *
 * Serves files from ext4 filesystem on VirtIO block device
 */

#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "lwip/ip4_addr.h"

#include "virtio_net.h"
#include "virtio_blk.h"
#include "fs.h"
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

/* MIME types */
struct mime_type {
    const char *ext;
    const char *type;
};

static const struct mime_type mime_types[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".txt", "text/plain"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".ico", "image/x-icon"},
    {".svg", "image/svg+xml"},
    {".bmp", "image/bmp"},
    {NULL, "application/octet-stream"}
};

/* Get MIME type from filename */
static const char *get_mime_type(const char *path) {
    const char *dot = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '.') dot = p;
    }
    if (dot) {
        for (int i = 0; mime_types[i].ext != NULL; i++) {
            const char *e = mime_types[i].ext;
            const char *d = dot;
            int match = 1;
            while (*e && *d) {
                char c1 = (*e >= 'A' && *e <= 'Z') ? *e + 32 : *e;
                char c2 = (*d >= 'A' && *d <= 'Z') ? *d + 32 : *d;
                if (c1 != c2) { match = 0; break; }
                e++; d++;
            }
            if (match && *e == 0 && *d == 0) return mime_types[i].type;
        }
    }
    return "application/octet-stream";
}

/* HTTP connection state */
#define HTTP_BUF_SIZE 4096
#define HTTP_PATH_SIZE 256

struct http_state {
    int sent_headers;
    int sent_body;
    fs_file_t file;
    int64_t file_size;
    int64_t bytes_sent;
    char path[HTTP_PATH_SIZE];
    uint8_t buf[HTTP_BUF_SIZE];
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

/* Format int64 to string */
static int int64_to_str(char *buf, int64_t val) {
    char tmp[24];
    int i = 0, j = 0;

    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    if (val < 0) {
        buf[j++] = '-';
        val = -val;
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

/* Parse URL path from HTTP request */
static int parse_url_path(const char *req, int len, char *path, int path_size) {
    /* Find start of path (after "GET ") */
    int i = 0;
    while (i < len && req[i] != ' ') i++;
    i++;  /* Skip space */

    if (i >= len) return -1;

    /* Copy path until space or ? or # */
    int j = 0;
    while (i < len && j < path_size - 1) {
        char c = req[i];
        if (c == ' ' || c == '?' || c == '#' || c == '\r' || c == '\n') break;
        path[j++] = c;
        i++;
    }
    path[j] = '\0';

    /* Handle root path */
    if (j == 1 && path[0] == '/') {
        /* Try index.html */
        memcpy(path, "/index.html", 12);
    }

    return 0;
}

/* Send file chunk callback */
static err_t http_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    struct http_state *hs = (struct http_state*)arg;
    (void)len;

    if (hs == NULL || hs->file == FS_INVALID_FILE) return ERR_OK;

    /* Read and send more data */
    ssize_t n = fs_read(hs->file, hs->buf, HTTP_BUF_SIZE);
    if (n > 0) {
        hs->bytes_sent += n;
        tcp_write(pcb, hs->buf, n, TCP_WRITE_FLAG_COPY);
        tcp_output(pcb);
    }

    /* Check if done */
    if (n <= 0 || hs->bytes_sent >= hs->file_size) {
        fs_close(hs->file);
        hs->file = FS_INVALID_FILE;
        tcp_close(pcb);
    }

    return ERR_OK;
}

/* TCP receive callback */
static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    struct http_state *hs = (struct http_state*)arg;

    if (p == NULL) {
        /* Connection closed by remote */
        if (hs && hs->file != FS_INVALID_FILE) {
            fs_close(hs->file);
        }
        tcp_close(pcb);
        if (hs) free(hs);
        return ERR_OK;
    }

    /* Acknowledge received data */
    tcp_recved(pcb, p->tot_len);

    /* Check if this is a GET request */
    char *data = (char*)p->payload;
    int plen = p->len;
    int is_get = (plen >= 3 && data[0] == 'G' && data[1] == 'E' && data[2] == 'T');

    if (!hs->sent_headers && is_get) {
        /* Parse the URL path */
        char path[HTTP_PATH_SIZE];
        if (parse_url_path(data, plen, path, HTTP_PATH_SIZE) != 0) {
            pbuf_free(p);
            tcp_write(pcb, http_404, sizeof(http_404) - 1, TCP_WRITE_FLAG_COPY);
            tcp_output(pcb);
            tcp_close(pcb);
            return ERR_OK;
        }

        console_printf("HTTP GET: %s\n", path);

        pbuf_free(p);

        /* Try to serve from filesystem first */
        int serve_from_disk = 0;
        if (fs_mounted()) {
            int64_t fsize = fs_stat_size(path);
            if (fsize >= 0) {
                hs->file = fs_open(path, FS_O_RDONLY);
                if (hs->file != FS_INVALID_FILE) {
                    hs->file_size = fsize;
                    hs->bytes_sent = 0;
                    serve_from_disk = 1;
                    console_printf("  -> Serving from disk (%lld bytes)\n", (long long)fsize);
                }
            }
        }

        if (serve_from_disk) {
            /* Serve file from disk */
            char header[512];
            int len = 0;

            /* HTTP status line */
            const char *ok = "HTTP/1.1 200 OK\r\n";
            memcpy(header + len, ok, strlen(ok));
            len += strlen(ok);

            /* Content-Type */
            const char *ct = "Content-Type: ";
            memcpy(header + len, ct, strlen(ct));
            len += strlen(ct);
            const char *mime = get_mime_type(path);
            memcpy(header + len, mime, strlen(mime));
            len += strlen(mime);
            header[len++] = '\r';
            header[len++] = '\n';

            /* Content-Length */
            const char *cl = "Content-Length: ";
            memcpy(header + len, cl, strlen(cl));
            len += strlen(cl);
            len += int64_to_str(header + len, hs->file_size);
            header[len++] = '\r';
            header[len++] = '\n';

            /* Connection close */
            const char *cc = "Connection: close\r\n\r\n";
            memcpy(header + len, cc, strlen(cc));
            len += strlen(cc);

            /* Send headers */
            tcp_write(pcb, header, len, TCP_WRITE_FLAG_COPY);
            hs->sent_headers = 1;

            /* Set up sent callback for chunked file transfer */
            tcp_sent(pcb, http_sent);

            /* Read and send first chunk */
            ssize_t n = fs_read(hs->file, hs->buf, HTTP_BUF_SIZE);
            if (n > 0) {
                hs->bytes_sent = n;
                tcp_write(pcb, hs->buf, n, TCP_WRITE_FLAG_COPY);
            }

            tcp_output(pcb);

            /* If file is small enough, close now */
            if (n <= 0 || hs->bytes_sent >= hs->file_size) {
                fs_close(hs->file);
                hs->file = FS_INVALID_FILE;
                tcp_close(pcb);
            }
        } else {
            /* Fall back to static HTML page */
            char header[256];
            int len = 0;

            memcpy(header + len, http_ok, sizeof(http_ok) - 1);
            len += sizeof(http_ok) - 1;
            len += int_to_str(header + len, sizeof(html_page) - 1);
            header[len++] = '\r';
            header[len++] = '\n';
            header[len++] = '\r';
            header[len++] = '\n';

            tcp_write(pcb, header, len, TCP_WRITE_FLAG_COPY);
            hs->sent_headers = 1;

            tcp_write(pcb, html_page, sizeof(html_page) - 1, TCP_WRITE_FLAG_COPY);
            hs->sent_body = 1;

            tcp_output(pcb);
            tcp_close(pcb);
        }
    } else if (!is_get) {
        pbuf_free(p);
        /* Send 404 for non-GET requests */
        tcp_write(pcb, http_404, sizeof(http_404) - 1, TCP_WRITE_FLAG_COPY);
        tcp_output(pcb);
        tcp_close(pcb);
    } else {
        pbuf_free(p);
    }

    return ERR_OK;
}

/* TCP error callback */
static void http_err(void *arg, err_t err) {
    struct http_state *hs = (struct http_state*)arg;
    (void)err;
    if (hs) {
        if (hs->file != FS_INVALID_FILE) {
            fs_close(hs->file);
        }
        free(hs);
    }
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

    hs->file = FS_INVALID_FILE;

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

    /* Initialize filesystem (optional - will work without disk) */
    if (fs_init() == 0) {
        console_printf("[OK] Filesystem mounted (ext4)\n");
    } else {
        console_printf("[--] No disk or filesystem not available\n");
        console_printf("     (Will serve static HTML only)\n");
    }

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
