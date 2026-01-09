/*
 * debug_bridge.c - Simple debug bridge for VirtIO FIFO testing
 *
 * Connects to Spike's Unix domain socket and prints/echoes Ethernet frames.
 * Does not provide actual network connectivity - use slirp_bridge for that.
 *
 * Build: gcc -O2 -o debug_bridge debug_bridge.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <fcntl.h>

#define MAX_FRAME_SIZE 2048
#define DEFAULT_SOCKET_PATH "/tmp/spike_fifo.sock"

static int spike_fd = -1;
static volatile int running = 1;

/* Receive buffer */
static uint8_t recv_buf[MAX_FRAME_SIZE * 4];
static size_t recv_buf_len = 0;

/* Print hex dump */
static void hex_dump(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("  %04zx: ", i);
        printf("%02x ", data[i]);
        if (i % 16 == 15 || i == len - 1) {
            /* Pad for last line */
            for (size_t j = i % 16; j < 15; j++) printf("   ");
            printf(" |");
            for (size_t j = i - (i % 16); j <= i; j++) {
                char c = data[j];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
            printf("|\n");
        }
    }
}

/* Parse and print Ethernet header */
static void print_ethernet(const uint8_t *frame, size_t len) {
    if (len < 14) {
        printf("Frame too short for Ethernet header\n");
        return;
    }

    printf("  Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           frame[0], frame[1], frame[2], frame[3], frame[4], frame[5]);
    printf("  Src MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           frame[6], frame[7], frame[8], frame[9], frame[10], frame[11]);

    uint16_t ethertype = (frame[12] << 8) | frame[13];
    printf("  EtherType: 0x%04x", ethertype);

    switch (ethertype) {
        case 0x0800: printf(" (IPv4)\n"); break;
        case 0x0806: printf(" (ARP)\n"); break;
        case 0x86DD: printf(" (IPv6)\n"); break;
        default: printf("\n"); break;
    }

    /* For ARP, print more details */
    if (ethertype == 0x0806 && len >= 42) {
        const uint8_t *arp = frame + 14;
        uint16_t oper = (arp[6] << 8) | arp[7];
        printf("  ARP %s:\n", oper == 1 ? "Request" : (oper == 2 ? "Reply" : "?"));
        printf("    Sender: %d.%d.%d.%d (%02x:%02x:%02x:%02x:%02x:%02x)\n",
               arp[14], arp[15], arp[16], arp[17],
               arp[8], arp[9], arp[10], arp[11], arp[12], arp[13]);
        printf("    Target: %d.%d.%d.%d (%02x:%02x:%02x:%02x:%02x:%02x)\n",
               arp[24], arp[25], arp[26], arp[27],
               arp[18], arp[19], arp[20], arp[21], arp[22], arp[23]);
    }

    /* For IPv4, print basic header */
    if (ethertype == 0x0800 && len >= 34) {
        const uint8_t *ip = frame + 14;
        printf("  IPv4: %d.%d.%d.%d -> %d.%d.%d.%d",
               ip[12], ip[13], ip[14], ip[15],
               ip[16], ip[17], ip[18], ip[19]);

        uint8_t proto = ip[9];
        switch (proto) {
            case 1: printf(" (ICMP)\n"); break;
            case 6: printf(" (TCP)\n"); break;
            case 17: printf(" (UDP)\n"); break;
            default: printf(" (proto=%d)\n", proto); break;
        }
    }
}

/* Create ARP reply for gateway IP */
static void send_arp_reply(const uint8_t *request, size_t len) {
    if (len < 42) return;

    const uint8_t *arp = request + 14;
    uint16_t oper = (arp[6] << 8) | arp[7];

    /* Only respond to ARP requests */
    if (oper != 1) return;

    /* Check if requesting gateway IP (10.0.2.2) */
    if (arp[24] != 10 || arp[25] != 0 || arp[26] != 2 || arp[27] != 2) {
        return;
    }

    printf(">>> Sending ARP reply for gateway\n");

    /* Build ARP reply */
    uint8_t reply[64];
    size_t reply_len = 42;

    /* Ethernet header */
    memcpy(reply, request + 6, 6);    /* Dst MAC = request Src MAC */
    reply[6] = 0x52;                   /* Src MAC = gateway MAC */
    reply[7] = 0x54;
    reply[8] = 0x00;
    reply[9] = 0x12;
    reply[10] = 0x35;
    reply[11] = 0x02;
    reply[12] = 0x08;                  /* EtherType = ARP */
    reply[13] = 0x06;

    /* ARP header */
    memcpy(reply + 14, arp, 4);        /* hw type, proto type */
    reply[18] = 6;                     /* hw size */
    reply[19] = 4;                     /* proto size */
    reply[20] = 0;                     /* operation = reply */
    reply[21] = 2;

    /* Sender = gateway */
    reply[22] = 0x52;
    reply[23] = 0x54;
    reply[24] = 0x00;
    reply[25] = 0x12;
    reply[26] = 0x35;
    reply[27] = 0x02;
    reply[28] = 10;
    reply[29] = 0;
    reply[30] = 2;
    reply[31] = 2;

    /* Target = requester */
    memcpy(reply + 32, arp + 8, 10);   /* Original sender MAC+IP */

    /* Send with length prefix */
    uint8_t frame[MAX_FRAME_SIZE];
    frame[0] = (reply_len >> 8) & 0xFF;
    frame[1] = reply_len & 0xFF;
    memcpy(frame + 2, reply, reply_len);

    ssize_t sent = send(spike_fd, frame, reply_len + 2, 0);
    if (sent < 0) {
        perror("send ARP reply");
    }
}

/* Connect to Spike */
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

    for (int i = 0; i < 60; i++) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            printf("Connected to Spike!\n");
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            return fd;
        }
        usleep(500000);
        if (i % 10 == 0) {
            printf("Waiting for Spike socket...\n");
        }
    }

    perror("connect");
    close(fd);
    return -1;
}

/* Handle input from Spike */
static void handle_spike_input(void) {
    ssize_t n = recv(spike_fd, recv_buf + recv_buf_len,
                     sizeof(recv_buf) - recv_buf_len, 0);

    if (n <= 0) {
        if (n == 0) {
            printf("Spike disconnected\n");
            running = 0;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv");
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
            break;
        }

        const uint8_t *frame = recv_buf + offset + 2;

        printf("\n=== RX Frame (%u bytes) ===\n", frame_len);
        print_ethernet(frame, frame_len);
        hex_dump(frame, frame_len > 64 ? 64 : frame_len);

        /* Auto-respond to ARP requests */
        uint16_t ethertype = (frame[12] << 8) | frame[13];
        if (ethertype == 0x0806) {
            send_arp_reply(frame, frame_len);
        }

        offset += 2 + frame_len;
    }

    if (offset > 0 && offset < recv_buf_len) {
        memmove(recv_buf, recv_buf + offset, recv_buf_len - offset);
        recv_buf_len -= offset;
    } else if (offset == recv_buf_len) {
        recv_buf_len = 0;
    }
}

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --socket=PATH   Spike VirtIO socket path (default: %s)\n", DEFAULT_SOCKET_PATH);
    printf("  --help          Show this help\n");
    printf("\nThis is a debug bridge that prints packets but does not provide\n");
    printf("actual network connectivity. For full networking, use slirp_bridge.\n");
}

int main(int argc, char *argv[]) {
    const char *socket_path = DEFAULT_SOCKET_PATH;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--socket=", 9) == 0) {
            socket_path = argv[i] + 9;
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
    printf("  Debug Bridge for Spike VirtIO\n");
    printf("=====================================\n");
    printf("Socket: %s\n", socket_path);
    printf("\n");
    printf("NOTE: This bridge provides packet debugging and basic ARP\n");
    printf("responses but NO actual network connectivity.\n");
    printf("Install libslirp-dev and use slirp_bridge for full networking:\n");
    printf("  sudo apt install libslirp-dev libglib2.0-dev\n");
    printf("\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    spike_fd = connect_to_spike(socket_path);
    if (spike_fd < 0) {
        return 1;
    }

    printf("Bridge running! Press Ctrl+C to stop\n\n");

    while (running) {
        fd_set rfds;
        struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};

        FD_ZERO(&rfds);
        FD_SET(spike_fd, &rfds);

        int ret = select(spike_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(spike_fd, &rfds)) {
            handle_spike_input();
        }
    }

    printf("\nShutting down...\n");
    if (spike_fd >= 0) close(spike_fd);
    printf("Done.\n");

    return 0;
}
