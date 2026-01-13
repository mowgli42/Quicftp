# Quicftp

File transfer over QUIC protocol - A C++ library and CLI tool for rapid, reliable file transfers using the QUIC transport protocol.

## Project Description

Quicftp provides a C++ client library and command-line tools for file transfer operations over the QUIC protocol. QUIC offers low-latency, multiplexed connections with built-in encryption, making it ideal for efficient file transfers with features like:

- Multiple parallel transfers over independent streams
- Built-in encryption and authentication
- Connection resilience and error recovery
- Optimized for modern network conditions

## Features

### Implemented
- ✅ QUIC/TLS handshake (client and server)
- ✅ Certificate-based authentication
- ✅ UDP-based QUIC packet transmission
- ✅ Connection lifecycle management
- ✅ Basic server implementation with verbose logging

### In Progress
- 🔄 File transfer over QUIC streams
- 🔄 Stream multiplexing for parallel transfers
- 🔄 Error recovery and retransmission

### Planned
- 📋 Stream prioritization
- 📋 0-RTT connection resumption
- 📋 Connection migration
- 📋 Flow control and congestion control
- 📋 Path MTU discovery
- 📋 Performance monitoring

For detailed feature specifications, see [OpenSpec documentation](openspec/specs/).

## Dependencies

### System Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- CMake 3.15 or later
- Linux, macOS, or Windows (Linux tested)

### Required Libraries
The following libraries must be installed on your system:

- **ngtcp2** - QUIC protocol implementation
- **ngtcp2_crypto_ossl** - OpenSSL integration for ngtcp2
- **nghttp3** - HTTP/3 layer (optional but recommended)
- **OpenSSL** - TLS/crypto backend (1.1.1 or later)

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libngtcp2-dev libngtcp2-crypto-ossl-dev libnghttp3-dev libssl-dev
```

#### Arch Linux
```bash
sudo pacman -S ngtcp2 nghttp3 openssl
```

#### macOS (Homebrew)
```bash
brew install ngtcp2 nghttp3 openssl
```

For more details on the QUIC library selection and integration, see [QUIC_LIBRARY.md](QUIC_LIBRARY.md).

## Building

### Quick Start
```bash
# Clone the repository
git clone <repository-url>
cd Quicftp

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)

# Or use the Makefile wrapper
cd ..
make
```

### Build Options
- `BUILD_CLIENT` (default: ON) - Build client library and CLI
- `BUILD_SERVER` (default: ON) - Build server library and CLI

Example:
```bash
cmake -DBUILD_SERVER=OFF ..
```

### Output
After building, you'll find:
- `build/quicftpclient` - Client CLI executable
- `build/quicftpserver` - Server executable
- `build/libquicftp_client.a` - Client library (static)
- `build/libquicftp_server.a` - Server library (static)

## Usage

### Certificate Setup

Before using Quicftp, you need to generate TLS certificates for testing:

```bash
# Generate test certificates
cd certs
./generate_certs.sh
```

This creates:
- `certs/server-cert.pem` and `certs/server-key.pem` - Server certificates
- `certs/client-cert.pem` and `certs/client-key.pem` - Client certificates

### Running the Server

Start the server on a specific port:

```bash
./build/quicftpserver <port> <cert-path> <key-path> <root-directory>

# Example
./build/quicftpserver 4433 certs/server-cert.pem certs/server-key.pem server_test_files
```

The server will:
- Listen for QUIC connections on the specified port
- Use the provided certificate and key for TLS
- Store uploaded files in the root directory
- Output verbose logging for debugging

### Using the Client

Upload files to the server:

```bash
./build/quicftpclient <server:port> upload <file1> <file2> ... <cert-path>

# Example
./build/quicftpclient 127.0.0.1:4433 upload test.txt certs/client-cert.pem
```

Download files from the server:

```bash
./build/quicftpclient <server:port> download <remote-path1> <remote-path2> ... <cert-path>

# Example
./build/quicftpclient 127.0.0.1:4433 download test.txt certs/client-cert.pem
```

### Programmatic API

The client library can be used in C++ code:

```cpp
#include "quicftp_client.h"

quicftp::Client client;

// Connect to server
if (!client.connect("127.0.0.1:4433")) {
    // Handle error
}

// Authenticate
if (!client.authenticate("certs/client-cert.pem")) {
    // Handle error
}

// Upload a file
if (!client.upload_file("local_file.txt", "remote_file.txt")) {
    // Handle error
}

// Disconnect
client.disconnect();
```

## Architecture

Quicftp uses a layered architecture:

```
┌─────────────────────────────────────┐
│   CLI (quicftpclient/quicftpserver) │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   Client/Server API (C++ Classes)   │
│   - quicftp::Client                 │
│   - quicftp::Server                 │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   QUIC Wrapper (quic_wrapper.h/cc)  │
│   - QuicClientWrapper               │
│   - QuicServerWrapper               │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   ngtcp2 Library (C API)            │
│   - QUIC Protocol                   │
│   - TLS via OpenSSL                 │
└─────────────────────────────────────┘
```

### Key Components

- **Client/Server Classes**: High-level C++ API for file operations
- **QUIC Wrapper**: C++ wrapper around ngtcp2 C API, handles connection management
- **Stream Manager**: Tracks individual file transfer streams
- **ngtcp2**: Low-level QUIC protocol implementation
- **OpenSSL**: TLS encryption and certificate handling

## Documentation

- **[OpenSpec Specifications](openspec/specs/)** - Detailed feature requirements
- **[QUIC Library Documentation](QUIC_LIBRARY.md)** - Library selection and integration details
- **[Project Context](openspec/project.md)** - Project conventions and tech stack
- **[OpenSpec Workflow](openspec/AGENTS.md)** - Development workflow guide

## License

MIT License - Copyright 2023 mowgli42

See LICENSE file for details.
