# RISC-V Bare-Metal Web Server Demo

A complete demonstration of a bare-metal HTTP web server running on the Spike RISC-V simulator, using lwIP TCP/IP stack and VirtIO FIFO for networking.

![Architecture](docs/architecture.png)

## Features

- **Bare-metal**: No operating system required
- **lwIP TCP/IP stack**: Full networking support
- **VirtIO FIFO**: Custom network device for Spike
- **VirtIO Block**: Virtual disk device with ext4 filesystem
- **lwext4**: Lightweight ext4 filesystem for embedded systems
- **SLIRP NAT**: User-mode networking (no root required)
- **Port forwarding**: Access guest web server from host
- **Static file serving**: Serve HTML, CSS, JS, images from virtual disk

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                              HOST SYSTEM                                  │
│                                                                           │
│  ┌──────────────────┐         ┌──────────────────────────────┐           │
│  │   Web Browser    │         │       slirp_bridge           │           │
│  │                  │  HTTP   │                              │           │
│  │  localhost:8080  │◄───────►│  - SLIRP NAT (10.0.2.0/24)  │           │
│  └──────────────────┘         │  - Port forward 8080→80     │           │
│                               └──────────────┬───────────────┘           │
│                                              │                            │
│               disk.img                Unix Domain Socket                  │
│              (ext4 fs)                       │                            │
│                  │                           │                            │
│  ┌───────────────┴───────────────────────────┴────────────────────────┐  │
│  │                         SPIKE SIMULATOR                             │  │
│  │  ┌──────────────────────────────────────────────────────────────┐  │  │
│  │  │                  RISC-V Guest (Bare-metal)                    │  │  │
│  │  │                                                               │  │  │
│  │  │  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐   │  │  │
│  │  │  │ HTTP Server │◄──►│    lwIP     │    │     lwext4      │   │  │  │
│  │  │  │  (port 80)  │    │   TCP/IP    │    │  ext4 filesystem│   │  │  │
│  │  │  └──────┬──────┘    └──────┬──────┘    └────────┬────────┘   │  │  │
│  │  │         │                  │                    │             │  │  │
│  │  │         │     Serve files from disk             │             │  │  │
│  │  │         └───────────────────────────────────────┘             │  │  │
│  │  │                            │                    │             │  │  │
│  │  │                 ┌──────────┴──────────┬─────────┴──────────┐  │  │  │
│  │  │                 │ VirtIO Net Driver   │ VirtIO Blk Driver  │  │  │  │
│  │  │                 └──────────┬──────────┴─────────┬──────────┘  │  │  │
│  │  └────────────────────────────┼────────────────────┼─────────────┘  │  │
│  │                               │                    │                 │  │
│  │                 ┌─────────────┴───────┐  ┌────────┴────────┐        │  │
│  │                 │ VirtIO FIFO Device  │  │ VirtIO Block    │        │  │
│  │                 │   (0x10001000)      │  │  (0x10002000)   │        │  │
│  │                 └─────────────────────┘  └─────────────────┘        │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────────┘
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
fs: Initializing filesystem...
VirtIO block device found
VirtIO block device initialized
ext4: Block device: ... blocks
fs: Filesystem mounted successfully
[OK] Filesystem mounted (ext4)
HTTP server listening on port 80

System ready! Access http://localhost:8080 from host.
```

## Disk Image Setup (Optional)

The web server can serve static files from an ext4-formatted disk image. This is optional - without a disk, it serves a built-in HTML page.

### Create Disk Image

```bash
# Create a 16MB ext4 disk image
dd if=/dev/zero of=disk.img bs=1M count=16
mkfs.ext4 -F disk.img
```

### Add Files to Disk

You can add files using `debugfs` (no root required):

```bash
# Create files on host
echo '<!DOCTYPE html>
<html>
<head><title>RISC-V Web Server</title></head>
<body>
  <h1>Hello from ext4!</h1>
  <p>Served from virtual disk!</p>
  <img src="/image.png" alt="Test Image">
</body>
</html>' > index.html

# Add files to disk image
debugfs -w disk.img << 'EOF'
write index.html /index.html
write image.png /image.png
quit
EOF
```

Or mount with root access:

```bash
sudo mount disk.img /mnt
sudo cp index.html /mnt/
sudo cp image.png /mnt/
sudo umount /mnt
```

### Run with Disk

```bash
spike --virtio-net=8080 --virtio-block=disk.img firmware/firmware.elf
```

### Supported File Types

| Extension | MIME Type |
|-----------|-----------|
| .html, .htm | text/html |
| .css | text/css |
| .js | application/javascript |
| .json | application/json |
| .png | image/png |
| .jpg, .jpeg | image/jpeg |
| .gif | image/gif |
| .svg | image/svg+xml |
| .ico | image/x-icon |
| .txt | text/plain |

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
├── firmware/                 # Guest bare-metal code
│   ├── src/                  # Source files
│   │   ├── main.c            # HTTP server with file serving
│   │   ├── virtio_net.c      # VirtIO network driver
│   │   ├── virtio_blk.c      # VirtIO block driver
│   │   ├── ext4_blockdev_virtio.c  # lwext4 block device adapter
│   │   ├── fs.c              # Filesystem API wrapper
│   │   ├── start.S           # Startup code
│   │   └── ...
│   ├── include/              # Headers
│   │   ├── ext4_config.h     # lwext4 configuration
│   │   └── ...
│   ├── lwip/                 # lwIP TCP/IP stack (submodule)
│   ├── lwext4/               # lwext4 filesystem (submodule)
│   ├── link.ld               # Linker script
│   └── Makefile
├── host/                     # Host-side bridge
│   ├── slirp_bridge.c        # SLIRP NAT bridge
│   ├── debug_bridge.c        # Debug packet monitor
│   └── Makefile
├── scripts/                  # Helper scripts
│   ├── build.sh
│   └── run.sh
└── README.md
```

## How It Works

1. **Spike** simulates a RISC-V processor with VirtIO devices (network + block)
2. **VirtIO FIFO** provides a byte-stream interface over a Unix socket
3. **VirtIO Block** provides disk access to a host file (disk.img)
4. **slirp_bridge** connects to the socket and provides NAT networking via SLIRP
5. **lwIP** runs on the guest, providing TCP/IP networking
6. **lwext4** mounts the ext4 filesystem from the virtual disk
7. **HTTP server** listens on port 80, serving files from disk or built-in HTML

## Customization

### Serve Custom Content

**Option 1: From Disk (Recommended)**

Add your HTML, CSS, JS, and image files to the disk image. The server will automatically serve them with correct MIME types.

```bash
# Add files to disk
debugfs -w disk.img -R "write my-page.html /my-page.html"
debugfs -w disk.img -R "write style.css /style.css"
debugfs -w disk.img -R "write app.js /app.js"
```

**Option 2: Built-in HTML**

Edit the fallback HTML in `firmware/src/main.c`:

```c
static const char html_page[] =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><title>My Custom Page</title></head>\n"
    "<body><h1>Hello World!</h1></body>\n"
    "</html>\n";
```

### Change Port Forwarding

Edit the run script or pass arguments:

```bash
./host/slirp_bridge --socket=/tmp/spike.sock --port=3000
```

## Related Projects

- [riscv-isa-sim](https://github.com/myftptoyman/riscv-isa-sim) - Spike with VirtIO FIFO & Block
- [lwIP](https://github.com/lwip-tcpip/lwip) - Lightweight TCP/IP stack
- [lwext4](https://github.com/gkostka/lwext4) - Lightweight ext4 filesystem library

## License

MIT License

## Credits

- lwIP TCP/IP stack by the lwIP developers
- lwext4 filesystem library by Grzegorz Kostka
- libslirp by the QEMU/libslirp developers
- Spike RISC-V simulator by the RISC-V Foundation
