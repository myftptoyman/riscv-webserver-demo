# Makefile for lwip-virtio bare-metal web server

CROSS ?= riscv64-linux-gnu-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

# Common flags for assembly and C
COMMON_FLAGS = -march=rv64g -mabi=lp64 -mcmodel=medany
COMMON_FLAGS += -O2 -g
COMMON_FLAGS += -ffreestanding -nostdlib -nostartfiles

# Assembly flags
ASFLAGS = $(COMMON_FLAGS)

# GCC include path for stdarg.h etc
GCC_INCLUDE = $(shell $(CC) -print-file-name=include)

# C compiler flags
CFLAGS = $(COMMON_FLAGS) -nostdinc
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -fno-builtin -fno-common
CFLAGS += -Iinclude
CFLAGS += -I$(GCC_INCLUDE)
CFLAGS += -Ilwip/src/include
CFLAGS += -Ilwext4/include
CFLAGS += -Isrc
CFLAGS += -DCONFIG_USE_DEFAULT_CFG=0

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

# lwext4 sources
LWEXT4_SRCS = \
    lwext4/src/ext4.c \
    lwext4/src/ext4_balloc.c \
    lwext4/src/ext4_bcache.c \
    lwext4/src/ext4_bitmap.c \
    lwext4/src/ext4_blockdev.c \
    lwext4/src/ext4_block_group.c \
    lwext4/src/ext4_crc32.c \
    lwext4/src/ext4_debug.c \
    lwext4/src/ext4_dir.c \
    lwext4/src/ext4_dir_idx.c \
    lwext4/src/ext4_extent.c \
    lwext4/src/ext4_fs.c \
    lwext4/src/ext4_hash.c \
    lwext4/src/ext4_ialloc.c \
    lwext4/src/ext4_inode.c \
    lwext4/src/ext4_super.c \
    lwext4/src/ext4_trans.c \
    lwext4/src/ext4_xattr.c

# Application sources
APP_SRCS = \
    src/main.c \
    src/virtio_net.c \
    src/virtio_blk.c \
    src/ext4_blockdev_virtio.c \
    src/fs.c \
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
SRCS = src/start.S $(LWIP_SRCS) $(LWEXT4_SRCS) $(APP_SRCS)

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
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).dump

.PHONY: all clean
