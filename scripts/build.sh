#!/bin/bash
#
# Build script for RISC-V bare-metal web server demo
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=========================================="
echo "  Building RISC-V Web Server Demo"
echo "=========================================="
echo ""

# Build firmware
echo "[1/2] Building firmware..."
cd "$PROJECT_DIR/firmware"
make clean
make
echo "      Firmware built: firmware/firmware.elf"
echo ""

# Build host bridge
echo "[2/2] Building host bridge..."
cd "$PROJECT_DIR/host"
make clean
make
echo "      Host bridge built"
echo ""

echo "=========================================="
echo "  Build complete!"
echo "=========================================="
echo ""
echo "To run the demo:"
echo "  ./scripts/run.sh"
echo ""
