# RISC-V Bare-Metal Web Server Demo

A complete demonstration of a bare-metal HTTP web server running on the Spike RISC-V simulator, using lwIP TCP/IP stack and VirtIO FIFO for networking.

![Architecture](docs/architecture.png)

## Features

- **Bare-metal**: No operating system required
- **lwIP TCP/IP stack**: Full networking support
- **VirtIO FIFO**: Custom network device for Spike
- **SLIRP NAT**: User-mode networking (no root required)
- **Port forwarding**: Access guest web server from host

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         HOST SYSTEM                              │
│                                                                  │
│  ┌──────────────────┐         ┌──────────────────────────────┐  │
│  │   Web Browser    │         │       slirp_bridge           │  │
│  │                  │  HTTP   │                              │  │
│  │  localhost:8080  │◄───────►│  - SLIRP NAT (10.0.2.0/24)  │  │
│  └──────────────────┘         │  - Port forward 8080→80     │  │
│                               │  - Unix socket connection    │  │
│                               └──────────────┬───────────────┘  │
│                                              │                   │
│                                     Unix Domain Socket           │
│                                              │                   │
│  ┌───────────────────────────────────────────┴───────────────┐  │
│  │                    SPIKE SIMULATOR                         │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │              RISC-V Guest (Bare-metal)              │  │  │
│  │  │                                                     │  │  │
│  │  │  ┌─────────────┐    ┌─────────────┐                │  │  │
│  │  │  │ HTTP Server │◄──►│    lwIP     │                │  │  │
│  │  │  │  (port 80)  │    │  TCP/IP     │                │  │  │
│  │  │  └─────────────┘    └──────┬──────┘                │  │  │
│  │  │                            │                        │  │  │
│  │  │                    ┌───────┴───────┐               │  │  │
│  │  │                    │ VirtIO Driver │               │  │  │
│  │  │                    └───────┬───────┘               │  │  │
│  │  └────────────────────────────┼────────────────────────┘  │  │
│  │                               │                            │  │
│  │                    ┌──────────┴──────────┐                │  │
│  │                    │  VirtIO FIFO Device │                │  │
│  │                    │    (0x10001000)     │                │  │
│  │                    └─────────────────────┘                │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install gcc-riscv64-linux-gnu libslirp-dev libglib2.0-dev

# Clone Spike with VirtIO FIFO support
git clone https://github.com/myftptoyman/riscv-isa-sim
cd riscv-isa-sim
mkdir build && cd build
../configure
make -j$(nproc)
export PATH=$PWD:$PATH
```

### Build & Run

```bash
# Clone this repository
git clone --recursive https://github.com/myftptoyman/riscv-webserver-demo
cd riscv-webserver-demo

# Build
./scripts/build.sh

# Run
./scripts/run.sh

# Test (in another terminal)
curl http://localhost:8080
```

### Expected Output

```
========================================
   RISC-V lwIP Web Server
========================================

[OK] Heap initialized
[OK] Timer initialized
[OK] PLIC initialized
[OK] lwIP initialized
VirtIO FIFO device found
VirtIO FIFO initialized
Network interface up: 10.0.2.15
[OK] Network interface ready
HTTP server listening on port 80

System ready! Access http://localhost:8080 from host.
```

## Network Configuration

| Setting | Value |
|---------|-------|
| Guest IP | 10.0.2.15 |
| Gateway | 10.0.2.2 |
| DNS | 10.0.2.3 |
| Host Port | 8080 |
| Guest Port | 80 |

## Project Structure

```
riscv-webserver-demo/
├── firmware/           # Guest bare-metal code
│   ├── src/           # Source files
│   │   ├── main.c     # HTTP server
│   │   ├── virtio_net.c # VirtIO driver
│   │   ├── start.S    # Startup code
│   │   └── ...
│   ├── include/       # Headers
│   ├── lwip/          # lwIP submodule
│   ├── link.ld        # Linker script
│   └── Makefile
├── host/              # Host-side bridge
│   ├── slirp_bridge.c # SLIRP NAT bridge
│   ├── debug_bridge.c # Debug packet monitor
│   └── Makefile
├── scripts/           # Helper scripts
│   ├── build.sh
│   └── run.sh
└── README.md
```

## How It Works

1. **Spike** simulates a RISC-V processor with a custom VirtIO FIFO device
2. **VirtIO FIFO** provides a byte-stream interface over a Unix socket
3. **slirp_bridge** connects to the socket and provides NAT networking via SLIRP
4. **lwIP** runs on the guest, providing TCP/IP networking
5. **HTTP server** listens on port 80, forwarded to host port 8080

## Customization

### Modify the web page

Edit `firmware/src/main.c`:

```c
static const char html_page[] =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><title>My Custom Page</title></head>\n"
    "<body><h1>Hello World!</h1></body>\n"
    "</html>\n";
```

### Change port forwarding

Edit the run script or pass arguments:

```bash
./host/slirp_bridge --socket=/tmp/spike.sock --port=3000
```

## Related Projects

- [riscv-isa-sim](https://github.com/myftptoyman/riscv-isa-sim) - Spike with VirtIO FIFO
- [lwip-virtio](https://github.com/myftptoyman/lwip-virtio) - Original lwIP port
- [lwIP](https://github.com/lwip-tcpip/lwip) - Lightweight TCP/IP stack

## License

MIT License

## Credits

- lwIP TCP/IP stack by the lwIP developers
- libslirp by the QEMU/libslirp developers
- Spike RISC-V simulator by the RISC-V Foundation
