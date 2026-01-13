# Build stage
FROM rust:1.70 as builder

WORKDIR /usr/src/app

# Copy manifest files
COPY Cargo.toml ./

# Copy source code
COPY src ./src

# Build the application in release mode
RUN cargo build --release

# Runtime stage
FROM debian:bookworm-slim

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y libssl3 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Copy the binary from builder
COPY --from=builder /usr/src/app/target/release/smarthome-pc-controller /usr/local/bin/

# Set environment variables with defaults
ENV DEVICE_MAC=00:00:00:00:00:00
ENV SERVER_IP=127.0.0.1
ENV HOME_API_KEY=""

# Expose the default port
EXPOSE 8080

# Run the application
CMD ["smarthome-pc-controller"]
