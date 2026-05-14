# Smart PC Controller

A lightweight Windows native application that allows remote shutdown of your PC via network commands. Built with pure Win32 API for minimal resource usage and maximum performance.

## Features

- **Remote Shutdown**: Shutdown your PC by sending a simple command over the network
- **Dual Protocol Support**: Listens on both UDP and TCP sockets simultaneously
- **System Tray Integration**: Runs silently in the background with a system tray icon
- **Single Instance Guard**: Prevents duplicate app instances from running at the same time
- **Configurable Port**: Change the listening port through an easy-to-use settings dialog
- **Configurable Shutdown Message**: Change the command message used to trigger shutdown
- **Auto-Start Option**: Configure the application to run automatically at Windows startup
- **Lightweight**: Uses less than 10 MB of RAM when idle
- **Single Executable**: No dependencies, just one standalone .exe file

## Requirements

- Windows 10 or later
- Administrator privileges (required for system shutdown)

## Installation

1. Download or compile `smart_pc_controller.exe`
2. Run the executable
3. The application will start and minimize to the system tray

## Usage

### Starting the Application

Simply run `smart_pc_controller.exe`. The application will:
- Start listening on port **10675** (default)
- Minimize to the system tray
- Display a tray icon with the Windows logo

### System Tray Menu

Right-click the tray icon to access:
- **Settings**: Configure port and startup options
- **Exit**: Close the application

### Settings

Click "Settings" from the tray menu to:
- **Change Port**: Set a custom listening port (1-65535)
- **Shutdown Message**: Set a custom command message (defaults to `shutdown-my-pc`)
- **Run at Startup**: Enable/disable automatic startup with Windows

### Remote Shutdown Command

To shutdown the PC remotely, send the configured shutdown message. The default is:
```
shutdown-my-pc
```

This command can be sent via:
- **UDP**: Send a UDP packet to the PC's IP address and configured port
- **TCP**: Establish a TCP connection and send the command

#### Example using netcat (UDP):
```bash
echo "shutdown-my-pc" | nc -u <PC_IP_ADDRESS> 10675
```

#### Example using netcat (TCP):
```bash
echo "shutdown-my-pc" | nc <PC_IP_ADDRESS> 10675
```

#### Example using Python (UDP):
```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"shutdown-my-pc", ("<PC_IP_ADDRESS>", 10675))
sock.close()
```

#### Example using Python (TCP):
```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("<PC_IP_ADDRESS>", 10675))
sock.send(b"shutdown-my-pc")
sock.close()
```

## Building from Source

### Prerequisites

- Microsoft Visual Studio 2019 or later
- Windows SDK

### Visual Studio

1. Open `SmartPCController.sln` in Visual Studio 2022 or later
2. Select `Debug` or `Release`, and `x64` or `Win32`
3. Press `F5` to build and run, or `Ctrl+F5` to run without debugging

Build outputs are written to:
```
bin\<Platform>\<Configuration>\SmartPCController.exe
```

### Command Line Compilation

1. Open Visual Studio Developer Command Prompt
2. Navigate to the project directory
3. Compile with:
```cmd
cl /EHsc /O2 /MT smart_pc_controller.cpp /link ws2_32.lib shell32.lib comctl32.lib user32.lib advapi32.lib
```

Or open the project in Visual Studio and build normally.

### Compiler Flags

- `/EHsc`: Enable C++ exception handling
- `/O2`: Optimize for speed
- `/MT`: Static link runtime library (creates standalone executable)

## Technical Details

### Architecture

- **Language**: C++ with Win32 API
- **Threading**: Multi-threaded design with separate UDP and TCP listener threads
- **Sockets**: Winsock2 for network communication
- **UI**: Native Win32 controls (no frameworks)

### Default Configuration

- **Default Port**: 10675
- **Buffer Size**: 1024 bytes
- **Socket Timeout**: 1 second
- **Registry Key**: `HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Run`
- **Default Shutdown Message**: `shutdown-my-pc`

### Security Considerations

⚠️ **Warning**: This application allows remote shutdown of your PC. Consider the following:

- Only use on trusted networks
- Consider using a firewall to restrict access to the listening port
- The shutdown command is a simple string without authentication
- For production use, implement additional security measures (authentication, encryption, etc.)

## Troubleshooting

### Application won't start
- Ensure you have administrator privileges
- Check if another application is using the configured port

### Remote shutdown not working
- Verify the PC's IP address and port number
- Check Windows Firewall settings
- Ensure the exact command string `shutdown-my-pc` is being sent
- Verify network connectivity between client and PC

### Port change not taking effect
- The application automatically restarts sockets when the port is changed
- If issues persist, try exiting and restarting the application

## License

This project is provided as-is for educational and personal use.

## Contributing

Contributions are welcome! Please ensure any modifications maintain the lightweight nature of the application and follow Win32 API best practices.

## Changelog

### Version 1.0
- Initial release
- UDP and TCP socket support
- System tray integration
- Configurable port
- Auto-startup option
- Remote shutdown functionality
