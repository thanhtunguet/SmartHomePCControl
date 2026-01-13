# SmartHomePCControl-Rust

A lightweight, high-performance Rust implementation of the SmartHomePCControl service using async/await and modern Rust patterns.

## Features

- 🚀 High-performance asynchronous HTTP server using Axum
- 🔐 Bearer token authentication for API security
- 🌐 Wake-on-LAN support for remote PC power-on
- 📡 Dual-mode shutdown commands (UDP + TCP)
- 🔍 Online status detection via RDP port probing
- 📊 Request logging with tracing

## Dependencies

- Rust 1.70 or higher
- Cargo (comes with Rust)

## Configuration

This project is configured using environment variables:

- `DEVICE_MAC`: The MAC address of the PC you want to control (format: `XX:XX:XX:XX:XX:XX` or `XX-XX-XX-XX-XX-XX`)
- `SERVER_IP`: The IP address of the PC you want to control
- `HOME_API_KEY`: API key for authentication (use as Bearer token)

## Building the Project

### Debug build

```sh
cargo build
```

### Release build (optimized)

```sh
cargo build --release
```

## Running the Application

### From source (debug)

Run on the default port (8080):
```sh
cargo run
```

Run on a custom port (e.g., 3000):
```sh
cargo run -- 3000
```

### From compiled binary (release)

After building with `cargo build --release`, run:

```sh
./target/release/smarthome-pc-controller
```

Or with a custom port:
```sh
./target/release/smarthome-pc-controller 3000
```

### With environment variables

```sh
DEVICE_MAC=AA:BB:CC:DD:EE:FF \
SERVER_IP=192.168.1.100 \
HOME_API_KEY=your-secret-key \
cargo run
```

## API Endpoints

All endpoints require Bearer token authentication via the `Authorization` header:

```
Authorization: Bearer <your-api-key>
```

### `GET /turn-on`

Sends a Wake-on-LAN magic packet to wake up the PC.

**Response:**
- `200 OK`: Magic packet sent successfully
- `401 Unauthorized`: Invalid or missing API key
- `500 Internal Server Error`: Failed to send magic packet

### `GET /turn-off`

Sends shutdown commands to the PC via both UDP and TCP.

**Response:**
- `200 OK`: Shutdown command sent successfully
- `401 Unauthorized`: Invalid or missing API key
- `500 Internal Server Error`: Failed to send shutdown command

### `GET /is-online`

Checks if the PC is online by attempting to connect to the RDP port (3389).

**Response:**
- `200 OK`: Returns `true` if online, `false` if offline
- `401 Unauthorized`: Invalid or missing API key

## Example Usage

```sh
# Turn on the PC
curl -H "Authorization: Bearer your-secret-key" http://localhost:8080/turn-on

# Turn off the PC
curl -H "Authorization: Bearer your-secret-key" http://localhost:8080/turn-off

# Check if PC is online
curl -H "Authorization: Bearer your-secret-key" http://localhost:8080/is-online
```

## Docker Support (Optional)

Create a `Dockerfile`:

```dockerfile
FROM rust:1.70 as builder
WORKDIR /usr/src/app
COPY . .
RUN cargo build --release

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y libssl3 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/src/app/target/release/smarthome-pc-controller /usr/local/bin/
CMD ["smarthome-pc-controller"]
```

Build and run:
```sh
docker build -t smarthome-pc-controller-rust .
docker run -p 8080:8080 \
  -e DEVICE_MAC=AA:BB:CC:DD:EE:FF \
  -e SERVER_IP=192.168.1.100 \
  -e HOME_API_KEY=your-secret-key \
  smarthome-pc-controller-rust
```

## Project Structure

```
SmartHomePCControl-Rust/
├── Cargo.toml          # Project dependencies and configuration
├── README.md           # This file
└── src/
    └── main.rs         # Main application code
```

## Key Technologies

- **Axum**: Modern, ergonomic web framework built on Tokio
- **Tokio**: Asynchronous runtime for Rust
- **Tower HTTP**: Middleware and utilities for HTTP services
- **Tracing**: Application-level tracing for logging
- **Custom WOL Implementation**: Native Wake-on-LAN magic packet generation

## Performance

The Rust implementation offers:
- Minimal memory footprint (~2-5 MB)
- Fast startup time (<100ms)
- Low CPU usage
- Efficient async I/O operations
- Native compilation for optimal performance

## License

Same as the parent SmartHomePCControl project.
