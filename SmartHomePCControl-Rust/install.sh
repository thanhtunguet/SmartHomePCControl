#!/bin/bash

# Installation script for SmartHomePCControl-Rust (Linux)

set -e

if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "Installing SmartHomePCControl-Rust..."

# Check if binary exists
if [ ! -f "target/release/smarthome-pc-controller" ]; then
    echo "Error: Binary not found. Please run './build.sh' first."
    exit 1
fi

# Copy binary
echo "Copying binary to /usr/local/bin/..."
cp target/release/smarthome-pc-controller /usr/local/bin/
chmod +x /usr/local/bin/smarthome-pc-controller

# Create working directory
echo "Creating working directory..."
mkdir -p /opt/smarthome-pc-controller

# Copy service file
echo "Installing systemd service..."
cp smarthome-pc-controller.service /etc/systemd/system/

# Prompt for configuration
echo ""
echo "Please edit the service file to configure your environment variables:"
echo "  sudo nano /etc/systemd/system/smarthome-pc-controller.service"
echo ""
echo "Set the following variables:"
echo "  - DEVICE_MAC: Your target PC's MAC address"
echo "  - SERVER_IP: Your target PC's IP address"
echo "  - HOME_API_KEY: Your secure API key"
echo ""
read -p "Press Enter to continue after editing the service file..."

# Reload systemd and enable service
systemctl daemon-reload
systemctl enable smarthome-pc-controller.service

echo ""
echo "Installation completed successfully!"
echo ""
echo "To start the service:"
echo "  sudo systemctl start smarthome-pc-controller"
echo ""
echo "To check status:"
echo "  sudo systemctl status smarthome-pc-controller"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u smarthome-pc-controller -f"
echo ""
