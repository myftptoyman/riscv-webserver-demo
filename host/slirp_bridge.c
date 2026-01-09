/*
 * slirp_bridge.c - Bridge between Spike VirtIO socket and SLIRP
 *
 * Connects to Spike's Unix domain socket and bridges network traffic
 * through SLIRP for user-mode NAT networking.
 *
 * Build: gcc -O2 -o slirp_bridge slirp_bridge.c $(pkg-config --cflags --libs slirp glib-2.0)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <slirp/libslirp.h>
#include <poll.h>

#define MAX_FRAME_SIZE 2048
#define MAX_POLL_FDS 64
#define DEFAULT_SOCKET_PATH "/tmp/spike_fifo.sock"
#define DEFAULT_HOST_PORT 8080
#define DEFAULT_GUEST_PORT 80

/* Global state */
static int spike_fd = -1;
static Slirp *slirp = NULL;
static volatile int running = 1;

/* Receive buffer for handling partial frames */
static uint8_t recv_buf[MAX_FRAME_SIZE * 4];
static size_t recv_buf_len = 0;

/* Poll fd management for SLIRP */
static struct pollfd poll_fds[MAX_POLL_FDS];
static int poll_fd_count = 0;

/* SLIRP callbacks */
static ssize_t slirp_send_packet(const void *buf, size_t len, void *opaque);
static void slirp_guest_error(const char *msg, void *opaque);
static int64_t slirp_clock_get_ns(void *opaque);
static void *slirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque);
static void slirp_timer_free(void *timer, void *opaque);
static void slirp_timer_mod(void *timer, int64_t expire_time, void *opaque);
static void slirp_register_poll_fd(int fd, void *opaque);
static void slirp_unregister_poll_fd(int fd, void *opaque);
static void slirp_notify(void *opaque);

static const SlirpCb slirp_callbacks = {
    .send_packet = slirp_send_packet,
    .guest_error = slirp_guest_error,
    .clock_get_ns = slirp_clock_get_ns,
    .timer_new = slirp_timer_new,
    .timer_free = slirp_timer_free,
    .timer_mod = slirp_timer_mod,
    .register_poll_fd = slirp_register_poll_fd,
    .unregister_poll_fd = slirp_unregister_poll_fd,
    .notify = slirp_notify,
};

/* Timer structure */
typedef struct SlirpTimer {
    SlirpTimerCb cb;
    void *cb_opaque;
    int64_t expire_time;
    struct SlirpTimer *next;
} SlirpTimer;

static SlirpTimer *timer_list = NULL;

/* Send Ethernet frame to guest via Spike socket */
static ssize_t slirp_send_packet(const void *buf, size_t len, void *opaque) {
    (void)opaque;

    if (spike_fd < 0 || len > MAX_FRAME_SIZE - 2) {
        return -1;
    }

    /* Frame format: 2-byte big-endian length prefix + frame data */
    uint8_t frame[MAX_FRAME_SIZE];
    frame[0] = (len >> 8) & 0xFF;
    frame[1] = len & 0xFF;
    memcpy(frame + 2, buf, len);

    ssize_t sent = send(spike_fd, frame, len + 2, 0);
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("send to spike");
        }
        return -1;
    }

    return len;
}

static void slirp_guest_error(const char *msg, void *opaque) {
    (void)opaque;
    fprintf(stderr, "SLIRP error: %s\n", msg);
}

static int64_t slirp_clock_get_ns(void *opaque) {
    (void)opaque;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void *slirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque) {
    (void)opaque;
    SlirpTimer *timer = malloc(sizeof(SlirpTimer));
    if (!timer) return NULL;

    timer->cb = cb;
    timer->cb_opaque = cb_opaque;
    timer->expire_time = -1;
    timer->next = timer_list;
    timer_list = timer;
    return timer;
}

static void slirp_timer_free(void *timer, void *opaque) {
    (void)opaque;
    SlirpTimer **pp = &timer_list;
    while (*pp) {
        if (*pp == timer) {
            *pp = (*pp)->next;
            free(timer);
            return;
        }
        pp = &(*pp)->next;
    }
}

static void slirp_timer_mod(void *timer, int64_t expire_time, void *opaque) {
    (void)opaque;
    if (timer) {
        ((SlirpTimer *)timer)->expire_time = expire_time;
    }
}

static void slirp_register_poll_fd(int fd, void *opaque) {
    (void)opaque;
    /* Add fd to poll set if not already there */
    for (int i = 0; i < poll_fd_count; i++) {
        if (poll_fds[i].fd == fd) return;
    }
    if (poll_fd_count < MAX_POLL_FDS) {
        poll_fds[poll_fd_count].fd = fd;
        poll_fds[poll_fd_count].events = 0;
        poll_fd_count++;
    }
}

static void slirp_unregister_poll_fd(int fd, void *opaque) {
    (void)opaque;
    /* Remove fd from poll set */
    for (int i = 0; i < poll_fd_count; i++) {
        if (poll_fds[i].fd == fd) {
            poll_fds[i] = poll_fds[--poll_fd_count];
            return;
        }
    }
}

static void slirp_notify(void *opaque) {
    (void)opaque;
}

/* Polling callbacks for SLIRP */
static int add_poll_cb(int fd, int events, void *opaque) {
    (void)opaque;
    if (poll_fd_count < MAX_POLL_FDS) {
        poll_fds[poll_fd_count].fd = fd;
        poll_fds[poll_fd_count].events = 0;
        if (events & SLIRP_POLL_IN) poll_fds[poll_fd_count].events |= POLLIN;
        if (events & SLIRP_POLL_OUT) poll_fds[poll_fd_count].events |= POLLOUT;
        poll_fds[poll_fd_count].revents = 0;
        return poll_fd_count++;
    }
    return -1;
}

static int get_revents_cb(int idx, void *opaque) {
    (void)opaque;
    if (idx < 0 || idx >= poll_fd_count) return 0;
    int revents = 0;
    if (poll_fds[idx].revents & POLLIN) revents |= SLIRP_POLL_IN;
    if (poll_fds[idx].revents & POLLOUT) revents |= SLIRP_POLL_OUT;
    if (poll_fds[idx].revents & POLLERR) revents |= SLIRP_POLL_ERR;
    if (poll_fds[idx].revents & POLLHUP) revents |= SLIRP_POLL_HUP;
    return revents;
}

/* Process expired timers */
static void process_timers(void) {
    int64_t now = slirp_clock_get_ns(NULL);
    SlirpTimer *t = timer_list;

    while (t) {
        if (t->expire_time >= 0 && t->expire_time <= now) {
            t->expire_time = -1;
            if (t->cb) {
                t->cb(t->cb_opaque);
            }
        }
        t = t->next;
    }
}

/* Connect to Spike's VirtIO socket */
static int connect_to_spike(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    printf("Connecting to %s...\n", path);

    /* Retry connection (Spike might not be ready yet) */
    for (int i = 0; i < 60; i++) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            printf("Connected to Spike!\n");

            /* Set non-blocking */
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            return fd;
        }
        usleep(500000); /* 500ms */
        if (i % 10 == 0) {
            printf("Waiting for Spike socket (%d seconds)...\n", (i + 1) / 2);
        }
    }

    perror("connect");
    close(fd);
    return -1;
}

/* Initialize SLIRP */
static Slirp *init_slirp(void) {
    SlirpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.version = 1;
    cfg.restricted = false;
    cfg.in_enabled = true;
    cfg.vnetwork.s_addr = inet_addr("10.0.2.0");
    cfg.vnetmask.s_addr = inet_addr("255.255.255.0");
    cfg.vhost.s_addr = inet_addr("10.0.2.2");        /* Gateway */
    cfg.vdhcp_start.s_addr = inet_addr("10.0.2.15"); /* DHCP start (guest IP) */
    cfg.vnameserver.s_addr = inet_addr("10.0.2.3");  /* DNS */

    return slirp_new(&cfg, &slirp_callbacks, NULL);
}

/* Handle input from Spike */
static void handle_spike_input(void) {
    /* Read available data */
    ssize_t n = recv(spike_fd, recv_buf + recv_buf_len,
                     sizeof(recv_buf) - recv_buf_len, 0);

    if (n <= 0) {
        if (n == 0) {
            printf("Spike disconnected\n");
            running = 0;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv from spike");
            running = 0;
        }
        return;
    }

    recv_buf_len += n;

    /* Process complete frames */
    size_t offset = 0;
    while (offset + 2 <= recv_buf_len) {
        uint16_t frame_len = (recv_buf[offset] << 8) | recv_buf[offset + 1];

        if (frame_len == 0 || frame_len > MAX_FRAME_SIZE) {
            fprintf(stderr, "Invalid frame length: %u\n", frame_len);
            recv_buf_len = 0;
            return;
        }

        if (offset + 2 + frame_len > recv_buf_len) {
            break; /* Incomplete frame */
        }

        /* Pass Ethernet frame to SLIRP */
        slirp_input(slirp, recv_buf + offset + 2, frame_len);
        offset += 2 + frame_len;
    }

    /* Move remaining data to beginning of buffer */
    if (offset > 0 && offset < recv_buf_len) {
        memmove(recv_buf, recv_buf + offset, recv_buf_len - offset);
        recv_buf_len -= offset;
    } else if (offset == recv_buf_len) {
        recv_buf_len = 0;
    }
}

/* Signal handler */
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

/* Print usage */
static void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --socket=PATH   Spike VirtIO socket path (default: %s)\n", DEFAULT_SOCKET_PATH);
    printf("  --port=PORT     Host port to forward to guest:80 (default: %d)\n", DEFAULT_HOST_PORT);
    printf("  --help          Show this help\n");
}

int main(int argc, char *argv[]) {
    const char *socket_path = DEFAULT_SOCKET_PATH;
    int host_port = DEFAULT_HOST_PORT;
    int guest_port = DEFAULT_GUEST_PORT;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--socket=", 9) == 0) {
            socket_path = argv[i] + 9;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            host_port = atoi(argv[i] + 7);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    printf("=====================================\n");
    printf("  SLIRP Bridge for Spike VirtIO\n");
    printf("=====================================\n");
    printf("Socket: %s\n", socket_path);
    printf("Port forwarding: localhost:%d -> guest:10.0.2.15:%d\n", host_port, guest_port);
    printf("\n");

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Initialize SLIRP */
    slirp = init_slirp();
    if (!slirp) {
        fprintf(stderr, "Failed to initialize SLIRP\n");
        return 1;
    }
    printf("SLIRP initialized (network: 10.0.2.0/24, gateway: 10.0.2.2)\n");

    /* Add port forwarding: host:8080 -> guest:80 */
    struct in_addr host_addr = {.s_addr = INADDR_ANY};
    struct in_addr guest_addr = {.s_addr = inet_addr("10.0.2.15")};

    if (slirp_add_hostfwd(slirp, 0, host_addr, host_port,
                          guest_addr, guest_port) < 0) {
        fprintf(stderr, "Warning: Failed to add port forwarding\n");
    } else {
        printf("Port forwarding: localhost:%d -> 10.0.2.15:%d\n", host_port, guest_port);
    }

    printf("\n");
    printf("Waiting for Spike to start...\n");
    printf("Run: spike --virtio-fifo=%s firmware.elf\n", socket_path);
    printf("\n");

    /* Connect to Spike */
    spike_fd = connect_to_spike(socket_path);
    if (spike_fd < 0) {
        fprintf(stderr, "Failed to connect to Spike\n");
        slirp_cleanup(slirp);
        return 1;
    }

    printf("\n");
    printf("Bridge running!\n");
    printf("Access web server at: http://localhost:%d\n", host_port);
    printf("Press Ctrl+C to stop\n");
    printf("\n");

    /* Main loop with proper SLIRP polling */
    while (running) {
        /* Reset poll fd events */
        poll_fd_count = 0;

        /* Add spike fd */
        poll_fds[poll_fd_count].fd = spike_fd;
        poll_fds[poll_fd_count].events = POLLIN;
        poll_fds[poll_fd_count].revents = 0;
        int spike_idx = poll_fd_count++;

        /* Let SLIRP add its FDs */
        uint32_t timeout = 10;
        slirp_pollfds_fill(slirp, &timeout, add_poll_cb, NULL);

        /* Poll all FDs */
        int ret = poll(poll_fds, poll_fd_count, timeout);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        /* Handle Spike input */
        if (poll_fds[spike_idx].revents & POLLIN) {
            handle_spike_input();
        }

        /* Let SLIRP process its events */
        slirp_pollfds_poll(slirp, ret <= 0, get_revents_cb, NULL);

        /* Process timers */
        process_timers();
    }

    printf("\nShutting down...\n");

    if (spike_fd >= 0) {
        close(spike_fd);
    }

    slirp_cleanup(slirp);

    /* Free all timers */
    while (timer_list) {
        SlirpTimer *t = timer_list;
        timer_list = t->next;
        free(t);
    }

    printf("Done.\n");
    return 0;
}
