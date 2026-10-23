### Build the Rust project
```sh
cd SmartHomePCControl-Rust
cargo build --release
```

### Build the C++ project
```sh
cd SmartHomePCControl-CPP
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Run with Docker
```sh
docker compose up -d
```