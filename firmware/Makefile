# Makefile for lwip-virtio bare-metal web server

CROSS ?= riscv64-linux-gnu-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

# Compiler flags
CFLAGS = -march=rv64g -mabi=lp64 -mcmodel=medany
CFLAGS += -O2 -g
CFLAGS += -ffreestanding -nostdlib -nostartfiles
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -fno-builtin -fno-common
CFLAGS += -Iinclude
CFLAGS += -Ilwip/src/include
CFLAGS += -Isrc

LDFLAGS = -T link.ld -nostdlib -static -Wl,--build-id=none

# lwIP sources
LWIP_CORE = \
    lwip/src/core/init.c \
    lwip/src/core/def.c \
    lwip/src/core/mem.c \
    lwip/src/core/memp.c \
    lwip/src/core/pbuf.c \
    lwip/src/core/stats.c \
    lwip/src/core/tcp.c \
    lwip/src/core/tcp_in.c \
    lwip/src/core/tcp_out.c \
    lwip/src/core/udp.c \
    lwip/src/core/ip.c \
    lwip/src/core/inet_chksum.c \
    lwip/src/core/timeouts.c

LWIP_IPV4 = \
    lwip/src/core/ipv4/etharp.c \
    lwip/src/core/ipv4/ip4.c \
    lwip/src/core/ipv4/ip4_addr.c \
    lwip/src/core/ipv4/ip4_frag.c \
    lwip/src/core/ipv4/icmp.c

LWIP_NETIF = \
    lwip/src/core/netif.c \
    lwip/src/netif/ethernet.c

LWIP_SRCS = $(LWIP_CORE) $(LWIP_IPV4) $(LWIP_NETIF)

# Application sources
APP_SRCS = \
    src/main.c \
    src/virtio_net.c \
    src/sys_arch.c \
    src/timer.c \
    src/heap.c \
    src/plic.c \
    src/trap.c \
    src/console.c \
    src/string.c \
    src/stdlib.c \
    src/printf.c

# All sources
SRCS = src/start.S $(LWIP_SRCS) $(APP_SRCS)

# Object files
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

# Targets
TARGET = firmware

all: $(TARGET).elf $(TARGET).bin $(TARGET).dump

$(TARGET).elf: $(OBJS) link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Built: $@"

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "Built: $@"

$(TARGET).dump: $(TARGET).elf
	$(OBJDUMP) -d $< > $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).dump

.PHONY: all clean
