# Quicftp

File transfer over QUIC protocol - A C++ library and CLI tool for rapid, reliable file transfers using the HTTP/3 protocol.

## Project Description

Quicftp provides a C++ client library and command-line tools for file transfer operations over the QUIC (HTTP/3) protocol. It leverages **Caddy** as a robust, production-grade QUIC server and **libcurl** for the client implementation.

## Features

- ✅ **HTTP/3 Support**: Uses QUIC transport via HTTP/3.
- ✅ **Secure**: TLS encryption enabled by default.
- ✅ **Parallel Transfers**: Multiple files can be transferred simultaneously.
- ✅ **Containerized Server**: Caddy-based server runs in Docker for easy deployment.
- ✅ **Simplified Architecture**: Removed complex custom QUIC stack in favor of industry standards.

## Dependencies

### System Requirements
- C++17 compatible compiler
- CMake 3.15 or later
- Docker (for running the server)

### Client Dependencies
- **libcurl** (built with HTTP/3 support: ngtcp2 + nghttp3 + openssl)

## Setup & Building

### 1. Build Client Dependencies
We provide a script to build a custom `libcurl` with HTTP/3 support enabled (as most system packages don't have it yet).

```bash
# Build openssl, ngtcp2, nghttp3, and curl locally
./scripts/build_curl_http3.sh
```

This will install dependencies into `deps/dist`.

### 2. Build the Project
```bash
mkdir build
cd build
cmake ..
make
```

### 3. Run the Server (Docker)
The server is a Caddy container configured for WebDAV/HTTP3 uploads.

```bash
# Build the server image
docker build -t quicftp-caddy ./docker/caddy

# Run the server (mapping port 443 UDP/TCP)
docker run -d -p 443:443/udp -p 443:443/tcp --name quicftp-server quicftp-caddy
```

## Usage

### Using the CLI

**Upload:**
```bash
./build/quicftpclient 127.0.0.1:443 upload local_file.txt certs/client-cert.pem
```

**Download:**
```bash
./build/quicftpclient 127.0.0.1:443 download remote_file.txt local_path.txt
```

### C++ API

```cpp
#include "quicftp_client.h"

quicftp::Client client;

// Connect to server
if (client.connect("https://127.0.0.1:443")) {
    // Authenticate (Client Cert)
    client.authenticate("certs/client-cert.pem", "certs/client-key.pem");

    // Upload
    client.upload_file("local.dat", "remote.dat");
    
    // Download
    client.download_file("remote.dat", "local_copy.dat");
    
    // Parallel Transfer
    std::vector<std::pair<std::string, std::string>> files = {
        {"file1.txt", "file1.txt"},
        {"file2.txt", "file2.txt"}
    };
    client.upload_files(files);
}
```

## Architecture

```
┌─────────────────────────┐      ┌─────────────────────────┐
│        Client           │      │         Server          │
│ (quicftp::Client + CLI) │      │   (Caddy Container)     │
│           ▼             │      │            ▼            │
│        libcurl          │◄────►│         WebDAV          │
│ (HTTP/3 + ngtcp2 + SSL) │ QUIC │   (HTTP/3 + TLS)        │
└─────────────────────────┘      └─────────────────────────┘
```

## Project Tracking

This project uses two complementary systems for development management:

- **[OpenSpec](openspec/)** — Formal specifications and change proposals for capability definitions
- **[Beads (bd)](https://github.com/steveyegge/beads)** — Git-backed issue tracker for persistent task management

### Quick Start with Beads
```bash
# Install bd CLI (once)
curl -fsSL https://raw.githubusercontent.com/steveyegge/beads/main/scripts/install.sh | bash

# View project task graph
bd graph workspace-sbe --compact

# Find ready work (no blockers)
bd ready

# View a specific task
bd show <task-id>
```

### Current Epics
| Epic | ID | Description |
|------|----|-------------|
| Project Tracking | `workspace-sbe` | Top-level project tracking |
| Beads Integration | `workspace-1zw` | Beads setup and configuration |
| Phase 2 | `workspace-n49` | Production-ready features (testing, security, error handling) |

## Documentation
- **[OpenSpec Specifications](openspec/specs/)** — Feature requirements and capabilities
- **[OpenSpec Changes](openspec/changes/)** — Active and archived change proposals
- **[Phase 2 Proposal](openspec/changes/add-phase2-production-ready/proposal.md)** — Production-ready features roadmap
- **[Beads Integration](openspec/changes/add-beads-project-tracking/proposal.md)** — Task tracking setup
- **[Architecture Refactor](openspec/changes/archive/2026-01-19-refactor-architecture-simplify/)** — Completed architecture shift

## License
MIT License - Copyright 2023 mowgli42
