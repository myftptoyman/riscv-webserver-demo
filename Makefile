# Top-level Makefile for RISC-V Web Server Demo

.PHONY: all firmware host clean run run-slirp test test-slirp spike spike-clone spike-clean

# Spike simulator path (use local clone by default)
SPIKE_REPO ?= https://github.com/myftptoyman/riscv-isa-sim.git
SPIKE_SRC ?= $(CURDIR)/riscv-isa-sim
SPIKE_BUILD ?= $(SPIKE_SRC)/build

# Default port for web server
PORT ?= 8080

all: spike firmware

firmware:
	@echo "Building firmware..."
	$(MAKE) -C firmware
	@echo ""

host:
	@echo "Building host bridge..."
	$(MAKE) -C host
	@echo ""

spike-clone:
	@if [ ! -d "$(SPIKE_SRC)" ]; then \
		echo "Cloning Spike simulator..."; \
		git clone $(SPIKE_REPO) $(SPIKE_SRC); \
	else \
		echo "Spike source already exists at $(SPIKE_SRC)"; \
	fi

spike: spike-clone
	@echo "Building Spike simulator..."
	@if [ ! -f "$(SPIKE_BUILD)/Makefile" ]; then \
		mkdir -p $(SPIKE_BUILD) && \
		cd $(SPIKE_BUILD) && ../configure; \
	fi
	$(MAKE) -C $(SPIKE_BUILD) -j$$(nproc)
	@echo ""
	@echo "Spike built: $(SPIKE_BUILD)/spike"

spike-clean:
	@if [ -d "$(SPIKE_BUILD)" ]; then \
		$(MAKE) -C $(SPIKE_BUILD) clean; \
	fi

clean:
	$(MAKE) -C firmware clean
	$(MAKE) -C host clean 2>/dev/null || true

# Run the demo with integrated SLIRP (recommended)
SPIKE ?= $(SPIKE_BUILD)/spike

run: all
	@echo "=========================================="
	@echo "  Starting RISC-V Web Server Demo"
	@echo "  (Integrated SLIRP networking)"
	@echo "=========================================="
	@echo ""
	@echo "Access web server at: http://localhost:$(PORT)"
	@echo "Press Ctrl+C to stop"
	@echo ""
	@$(SPIKE) --virtio-net=$(PORT) firmware/firmware.elf

test: all
	@echo "=========================================="
	@echo "  Testing RISC-V Web Server"
	@echo "  (Integrated SLIRP networking)"
	@echo "=========================================="
	@$(SPIKE) --virtio-net=$(PORT) firmware/firmware.elf & \
	SPIKE_PID=$$!; \
	sleep 5; \
	echo ""; \
	echo "Testing with curl:"; \
	curl -s http://localhost:$(PORT)/ && echo ""; \
	echo ""; \
	echo "Test passed!"; \
	kill $$SPIKE_PID 2>/dev/null

# Legacy targets using external slirp_bridge (for older spike versions)
SOCKET ?= /tmp/spike_virtio.sock

run-bridge: all host
	@echo "=========================================="
	@echo "  Starting RISC-V Web Server Demo"
	@echo "  (External SLIRP bridge)"
	@echo "=========================================="
	@echo ""
	@echo "Starting Spike and bridge..."
	@rm -f $(SOCKET)
	@$(SPIKE) --virtio-fifo=$(SOCKET) firmware/firmware.elf &
	@sleep 2
	@./host/slirp_bridge --socket=$(SOCKET) --port=$(PORT)

test-bridge: all host
	@echo "=========================================="
	@echo "  Testing RISC-V Web Server"
	@echo "  (External SLIRP bridge)"
	@echo "=========================================="
	@rm -f $(SOCKET)
	@$(SPIKE) --virtio-fifo=$(SOCKET) firmware/firmware.elf & \
	SPIKE_PID=$$!; \
	sleep 2; \
	./host/slirp_bridge --socket=$(SOCKET) --port=$(PORT) & \
	BRIDGE_PID=$$!; \
	sleep 3; \
	echo ""; \
	echo "Testing with curl:"; \
	curl -s http://localhost:$(PORT)/ && echo ""; \
	echo ""; \
	echo "Test passed!"; \
	kill $$SPIKE_PID $$BRIDGE_PID 2>/dev/null; \
	rm -f $(SOCKET)

help:
	@echo "RISC-V Web Server Demo"
	@echo ""
	@echo "Usage:"
	@echo "  make            - Build spike and firmware"
	@echo "  make firmware   - Build only firmware"
	@echo "  make host       - Build external SLIRP bridge (legacy)"
	@echo "  make spike-clone- Clone Spike source from GitHub"
	@echo "  make spike      - Clone (if needed) and build Spike simulator"
	@echo "  make spike-clean- Clean Spike build"
	@echo "  make clean      - Clean build files"
	@echo "  make run        - Run demo with integrated SLIRP (recommended)"
	@echo "  make test       - Build, run, and test with curl"
	@echo ""
	@echo "Legacy (external bridge):"
	@echo "  make run-bridge - Run demo with external SLIRP bridge"
	@echo "  make test-bridge- Test with external SLIRP bridge"
	@echo ""
	@echo "Variables:"
	@echo "  PORT=8080             - Host port for web server"
	@echo "  SPIKE=/path/to/spike  - Path to spike binary"
	@echo "  SPIKE_SRC=/path       - Path to Spike source"
	@echo "  SPIKE_REPO=url        - Git repo URL for Spike"
