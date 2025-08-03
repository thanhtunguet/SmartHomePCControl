# SmartHomePCControl-CPP

A lightweight, cross-platform C++ implementation of the SmartHomePCControl service.

## Dependencies

- A C++17 compatible compiler (e.g., g++, clang)
- CMake (version 3.10 or higher)

## Configuration

This project is configured using environment variables:

- `DEVICE_MAC`: The MAC address of the PC you want to control.
- `SERVER_IP`: The IP address of the PC you want to control.

## Building the Project

1.  Create a `build` directory:

    ```sh
    mkdir build
    cd build
    ```

2.  Run CMake to configure the project:

    ```sh
    cmake ..
    ```

3.  Compile the project:

    ```sh
    make
    ```

## Running the Application

After building, you can run the application from the `build` directory. You can optionally provide a port number as a command-line argument.

Run on the default port (8080):
```sh
./smarthome_pc_controller
```

Run on a custom port (e.g., 3000):
```sh
./smarthome_pc_controller 3000
```
