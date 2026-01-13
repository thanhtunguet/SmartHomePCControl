#!/bin/bash

# Build script for SmartHomePCControl-Rust

set -e

echo "Building SmartHomePCControl-Rust..."

# Check if Rust is installed
if ! command -v cargo &> /dev/null; then
    echo "Error: Rust/Cargo is not installed. Please install Rust from https://rustup.rs/"
    exit 1
fi

# Build in release mode
echo "Building release binary..."
cargo build --release

echo ""
echo "Build completed successfully!"
echo "Binary location: target/release/smarthome-pc-controller"
echo ""
echo "To run the application:"
echo "  ./target/release/smarthome-pc-controller [port]"
echo ""
echo "To install system-wide (Linux):"
echo "  sudo cp target/release/smarthome-pc-controller /usr/local/bin/"
echo ""
