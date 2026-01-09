# Top-level Makefile for RISC-V Web Server Demo

.PHONY: all firmware host clean run test spike spike-clone spike-clean

# Spike simulator path (use local clone by default)
SPIKE_REPO ?= https://github.com/myftptoyman/riscv-isa-sim.git
SPIKE_SRC ?= $(CURDIR)/riscv-isa-sim
SPIKE_BUILD ?= $(SPIKE_SRC)/build

all: spike firmware host

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
	$(MAKE) -C host clean

# Run the demo (requires spike with VirtIO FIFO support)
SPIKE ?= $(SPIKE_BUILD)/spike
SOCKET ?= /tmp/spike_virtio.sock

run: all
	@echo "=========================================="
	@echo "  Starting RISC-V Web Server Demo"
	@echo "=========================================="
	@echo ""
	@echo "Starting Spike and bridge..."
	@rm -f $(SOCKET)
	@$(SPIKE) --virtio-fifo=$(SOCKET) firmware/firmware.elf &
	@sleep 2
	@./host/slirp_bridge --socket=$(SOCKET) --port=8080

test: all
	@echo "=========================================="
	@echo "  Testing RISC-V Web Server"
	@echo "=========================================="
	@rm -f $(SOCKET)
	@$(SPIKE) --virtio-fifo=$(SOCKET) firmware/firmware.elf & \
	SPIKE_PID=$$!; \
	sleep 2; \
	./host/slirp_bridge --socket=$(SOCKET) --port=8080 & \
	BRIDGE_PID=$$!; \
	sleep 3; \
	echo ""; \
	echo "Testing with curl:"; \
	curl -s http://localhost:8080/ && echo ""; \
	echo ""; \
	echo "Test passed!"; \
	kill $$SPIKE_PID $$BRIDGE_PID 2>/dev/null; \
	rm -f $(SOCKET)

help:
	@echo "RISC-V Web Server Demo"
	@echo ""
	@echo "Usage:"
	@echo "  make            - Build firmware and host bridge"
	@echo "  make firmware   - Build only firmware"
	@echo "  make host       - Build only host bridge"
	@echo "  make spike-clone- Clone Spike source from GitHub"
	@echo "  make spike      - Clone (if needed) and build Spike simulator"
	@echo "  make spike-clean- Clean Spike build"
	@echo "  make clean      - Clean firmware and host build files"
	@echo "  make run        - Run the demo (Ctrl+C to stop)"
	@echo "  make test       - Build, run, and test with curl"
	@echo ""
	@echo "Variables:"
	@echo "  SPIKE=/path/to/spike      - Path to spike binary"
	@echo "  SPIKE_SRC=/path/to/source - Path to Spike source"
	@echo "  SPIKE_REPO=url            - Git repo URL for Spike"
	@echo "  SOCKET=/path/to/sock      - Unix socket path"
