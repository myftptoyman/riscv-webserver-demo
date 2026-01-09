# Makefile for host bridges

CC = gcc
CFLAGS = -O2 -Wall -Wextra

# Check if libslirp is available
SLIRP_AVAILABLE := $(shell pkg-config --exists slirp glib-2.0 && echo yes)

ifeq ($(SLIRP_AVAILABLE),yes)
TARGETS = slirp_bridge debug_bridge
SLIRP_CFLAGS = $(shell pkg-config --cflags slirp glib-2.0)
SLIRP_LDFLAGS = $(shell pkg-config --libs slirp glib-2.0)
else
TARGETS = debug_bridge
endif

all: $(TARGETS)
	@echo ""
	@echo "Built: $(TARGETS)"
ifeq ($(SLIRP_AVAILABLE),)
	@echo ""
	@echo "NOTE: slirp_bridge not built (libslirp not found)"
	@echo "Install with: sudo apt install libslirp-dev libglib2.0-dev"
endif

slirp_bridge: slirp_bridge.c
	$(CC) $(CFLAGS) $(SLIRP_CFLAGS) -o $@ $< $(SLIRP_LDFLAGS)

debug_bridge: debug_bridge.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f slirp_bridge debug_bridge

.PHONY: all clean
