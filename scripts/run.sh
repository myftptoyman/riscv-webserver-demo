#!/bin/bash
#
# Run script for RISC-V bare-metal web server demo
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SOCKET_PATH="/tmp/spike_virtio.sock"

# Find spike
SPIKE="${SPIKE:-$(which spike 2>/dev/null)}"
if [ -z "$SPIKE" ]; then
    # Try common locations
    for path in \
        "/home/toyman/riscv-isa-sim/build/spike" \
        "/usr/local/bin/spike" \
        "/opt/riscv/bin/spike"; do
        if [ -x "$path" ]; then
            SPIKE="$path"
            break
        fi
    done
fi

if [ -z "$SPIKE" ] || [ ! -x "$SPIKE" ]; then
    echo "ERROR: spike not found!"
    echo "Please set SPIKE environment variable or install spike."
    echo ""
    echo "To build spike with VirtIO FIFO support:"
    echo "  git clone https://github.com/myftptoyman/riscv-isa-sim"
    echo "  cd riscv-isa-sim && mkdir build && cd build"
    echo "  ../configure && make -j\$(nproc)"
    exit 1
fi

# Check if firmware exists
if [ ! -f "$PROJECT_DIR/firmware/firmware.elf" ]; then
    echo "ERROR: firmware.elf not found!"
    echo "Please run ./scripts/build.sh first."
    exit 1
fi

# Check if bridge exists
BRIDGE="$PROJECT_DIR/host/slirp_bridge"
if [ ! -x "$BRIDGE" ]; then
    BRIDGE="$PROJECT_DIR/host/debug_bridge"
    if [ ! -x "$BRIDGE" ]; then
        echo "ERROR: No bridge found!"
        echo "Please run ./scripts/build.sh first."
        exit 1
    fi
    echo "NOTE: Using debug_bridge (no network connectivity)"
    echo "      Install libslirp-dev for full networking"
fi

# Cleanup
cleanup() {
    echo ""
    echo "Shutting down..."
    kill $SPIKE_PID $BRIDGE_PID 2>/dev/null
    rm -f "$SOCKET_PATH"
    exit 0
}
trap cleanup SIGINT SIGTERM

# Remove old socket
rm -f "$SOCKET_PATH"

echo "=========================================="
echo "  RISC-V Bare-Metal Web Server Demo"
echo "=========================================="
echo ""
echo "Spike: $SPIKE"
echo "Bridge: $BRIDGE"
echo "Socket: $SOCKET_PATH"
echo ""

# Start Spike
echo "Starting Spike simulator..."
"$SPIKE" --virtio-fifo="$SOCKET_PATH" "$PROJECT_DIR/firmware/firmware.elf" &
SPIKE_PID=$!
sleep 2

# Start bridge
echo "Starting network bridge..."
"$BRIDGE" --socket="$SOCKET_PATH" --port=8080 &
BRIDGE_PID=$!
sleep 2

echo ""
echo "=========================================="
echo "  Web server is running!"
echo "=========================================="
echo ""
echo "  Access: http://localhost:8080"
echo ""
echo "  Try: curl http://localhost:8080"
echo ""
echo "  Press Ctrl+C to stop"
echo ""

# Wait for processes
wait $SPIKE_PID $BRIDGE_PID
