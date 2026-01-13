SmartHomePCControl
------------------

Control your Home PC using ~~Google Smart Home assistant~~ Web APIs

## Available Implementations

This project provides multiple implementations to suit different needs:

| Implementation | Language | Framework | Binary Size | Best For |
|----------------|----------|-----------|-------------|----------|
| **SmartHomePCControl** | C# | ASP.NET Core | ~100MB | Full-featured, cross-platform |
| **SmartHomePCControl-CPP** | C++ | cpp-httplib | ~2-3MB | Lightweight, minimal dependencies |
| **SmartHomePCControl-Rust** | Rust | Axum/Tokio | ~1.1MB | Memory-safe, high concurrency |
| **SmartHomePCControl-Windows** | C++ | Windows API | ~500KB | Windows-specific |

All implementations provide the same REST API endpoints and can be used interchangeably.

## Installation

### Quick setup --- if you've done this kind of thing before

[ Set up in Desktop](x-github-client://openRepo/https://github.com/thanhtunguet/SmartHomePCControl)

### ...or create a new repository on the command line

```sh
echo "# SmartHomePCControl" >> README.md
git init
git add README.md
git commit -m "first commit"
git branch -M main
git remote add origin git@github.com:thanhtunguet/SmartHomePCControl.git
git push -u origin main
```

### ...or push an existing repository from the command line

```sh
git remote add origin git@github.com:thanhtunguet/SmartHomePCControl.git
git branch -M main
git push -u origin main
```

### ...or import code from another repository

You can initialize this repository with code from a Subversion, Mercurial, or TFS project.

[Import code](https://github.com/thanhtunguet/SmartHomePCControl/import)

## Configuration

This project is configured using environment variables. You can create a `.env` file in the `SmartHomePCControl` directory to store these variables. The following variables are required:

- `DEVICE_MAC`: The MAC address of the PC you want to control.
- `SERVER_IP`: The IP address of the PC you want to control.

Example `.env` file:

```
DEVICE_MAC=00:11:22:33:44:55
SERVER_IP=192.168.1.100
```

## Quick Start

### Using Docker (Recommended)

**C# Version:**
```bash
docker compose up -d
```

Or manually:
```yml
version: '3'
services:
  smart-home-pc:
    image: thanhtunguet/smart-home-pc:latest
    network_mode: host
    container_name: smart-home-pc
    command: dotnet SmartHomePCControl.dll --urls http://0.0.0.0:5000
```

**Rust Version:**
```bash
cd SmartHomePCControl-Rust
docker build -t smarthome-pc-rust .
docker run -p 8080:8080 \
  -e DEVICE_MAC=AA:BB:CC:DD:EE:FF \
  -e SERVER_IP=192.168.1.100 \
  -e HOME_API_KEY=your-secret-key \
  smarthome-pc-rust
```

### Building from Source

**C# (.NET):**
```bash
cd SmartHomePCControl
dotnet build
dotnet run
```

**C++:**
```bash
cd SmartHomePCControl-CPP
mkdir build && cd build
cmake ..
make
./smarthome_pc_controller
```

**Rust:**
```bash
cd SmartHomePCControl-Rust
cargo build --release
./target/release/smarthome-pc-controller
```

## API Endpoints

All implementations expose the same REST API:

- `GET /turn-on` - Send Wake-on-LAN magic packet
- `GET /turn-off` - Send shutdown command
- `GET /is-online` - Check if PC is online

Authentication: Bearer token via `Authorization` header

## Implementation Details

For detailed information about each implementation:
- [C# Version](./SmartHomePCControl/)
- [C++ Version](./SmartHomePCControl-CPP/)
- [Rust Version](./SmartHomePCControl-Rust/)
- [C++/Rust Comparison](./SmartHomePCControl-Rust/COMPARISON.md)
