SmartHomePCControl
------------------

Control your Home PC using ~~Google Smart Home assistant~~ Web APIs

## Available Implementations

This project provides multiple implementations to suit different needs:

| Implementation                 | Language | Framework   | Binary Size | Best For                                        |
|--------------------------------|----------|-------------|-------------|-------------------------------------------------|
| **SmartHomePCControl-Rust**    | Rust     | Axum/Tokio  | ~1.1MB      | Memory-safe, high concurrency, production-ready |
| **SmartHomePCControl-CPP**     | C++      | cpp-httplib | ~2-3MB      | Lightweight, minimal dependencies               |
| **SmartHomePCControl-Windows** | C++      | Windows API | ~500KB      | Windows-specific                                |

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

This project is configured using environment variables. You can create a `.env` file in the project root to store these variables. The following variables are required:

- `DEVICE_MAC`: The MAC address of the PC you want to control.
- `SERVER_IP`: The IP address of the PC you want to control.
- `HOME_API_KEY`: API key for authenticating incoming requests (also accepts `WEB_API_KEY`).

Example `.env` file:

```
DEVICE_MAC=00:11:22:33:44:55
SERVER_IP=192.168.1.100
HOME_API_KEY=your-secret-api-key
```

## Quick Start

### Using Docker (Recommended)

Start the Rust service via Docker Compose:

```bash
docker compose up -d
```

Or build and run the Docker image manually:

```bash
docker build -t smarthome-pc-rust ./SmartHomePCControl-Rust
docker run -p 8080:8080 \
  -e DEVICE_MAC=AA:BB:CC:DD:EE:FF \
  -e SERVER_IP=192.168.1.100 \
  -e HOME_API_KEY=your-secret-key \
  smarthome-pc-rust
```

### Building from Source

**Rust (Recommended):**
```bash
cd SmartHomePCControl-Rust
cargo build --release
./target/release/smarthome-pc-controller
```

**C++:**
```bash
cd SmartHomePCControl-CPP
mkdir build && cd build
cmake ..
make
./smarthome_pc_controller
```

## API Endpoints

All implementations expose the same REST API:

- `GET /turn-on` - Send Wake-on-LAN magic packet
- `GET /turn-off` - Send shutdown command
- `GET /is-online` - Check if PC is online

Authentication: Bearer token via `Authorization` header

## Implementation Details

For detailed information about each implementation:
- [Rust Version](./SmartHomePCControl-Rust/)
- [C++ Version](./SmartHomePCControl-CPP/)
- [C++/Rust Comparison](./SmartHomePCControl-Rust/COMPARISON.md)
