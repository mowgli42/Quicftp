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

### 1. Build Client Dependencies (Optional)
For full HTTP/3 (QUIC) support, build a custom `libcurl` with the HTTP/3 backend. If you skip this step, the project will build with your system libcurl and fall back to HTTP/2.

```bash
# Build openssl (quictls), ngtcp2, nghttp3, and curl locally
./scripts/build_curl_http3.sh
```

This will install dependencies into `deps/dist`.

### 2. Build the Project
```bash
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=g++ ..
make
```

Example output:
```
-- Using system libcurl
-- Found CURL: /usr/lib/x86_64-linux-gnu/libcurl.so (found version "8.5.0")
-- Configuring done (0.2s)
-- Generating done (0.0s)
[ 25%] Building CXX object CMakeFiles/quicftp_client.dir/quicftp_client.cc.o
[ 50%] Linking CXX static library libquicftp_client.a
[ 75%] Building CXX object CMakeFiles/quicftpclient.dir/quicftpclient-cli.cc.o
[100%] Linking CXX executable quicftpclient
```

### 3. Run the Server (Docker)
The server is a Caddy container configured for WebDAV/HTTP3 uploads.

```bash
# Build the server image
docker build -t quicftp-caddy ./docker/caddy

# Run the server (mapping port 443 UDP/TCP and port 80)
docker run -d -p 443:443/udp -p 443:443/tcp -p 80:80 --name quicftp-server quicftp-caddy
```

Server starts with HTTP/1.1, HTTP/2, and HTTP/3 support:
```
{"logger":"http.log","msg":"server running","name":"srv0","protocols":["h1","h2","h3"]}
{"logger":"http","msg":"enabling HTTP/3 listener","addr":":443"}
```

## Usage

### CLI Reference
```
$ ./build/quicftpclient --help
Usage: ./build/quicftpclient [options] <server> <upload|download> <file1> [file2 ...]

Options:
  --help              Show this help message
  --version           Show version
  --verbose           Enable verbose output
  --insecure          Disable TLS certificate verification
  --ca-cert <path>    Path to CA certificate for TLS verification
  --cert <path>       Path to client certificate
  --key <path>        Path to client private key
  --progress          Show transfer progress

$ ./build/quicftpclient --version
quicftpclient 0.2.0
```

### Single File Upload
```
$ echo "Hello from quicftp!" > test_hello.txt
$ ./build/quicftpclient localhost:443 upload test_hello.txt
Uploaded test_hello.txt (20 bytes)
```

### Single File Download
```
$ rm test_hello.txt
$ ./build/quicftpclient localhost:443 download test_hello.txt
Downloaded test_hello.txt (20 bytes)
$ cat test_hello.txt
Hello from quicftp!
```

### Parallel Upload (Multiple Files)
```
$ echo "File Alpha" > test_a.txt && echo "File Bravo" > test_b.txt && echo "File Charlie" > test_c.txt
$ ./build/quicftpclient localhost:443 upload test_a.txt test_b.txt test_c.txt
Uploaded test_a.txt (11 bytes)
Uploaded test_b.txt (11 bytes)
Uploaded test_c.txt (13 bytes)
3/3 files uploaded successfully
```

### Parallel Download (Multiple Files)
```
$ rm test_a.txt test_b.txt test_c.txt
$ ./build/quicftpclient localhost:443 download test_a.txt test_b.txt test_c.txt
Downloaded test_a.txt (11 bytes)
Downloaded test_b.txt (11 bytes)
Downloaded test_c.txt (13 bytes)
3/3 files downloaded successfully
```

### Large File Transfer (5MB, checksum verified)
```
$ dd if=/dev/urandom of=test_5mb.bin bs=1M count=5
5242880 bytes (5.2 MB, 5.0 MiB) copied, 0.011 s, 459 MB/s

$ ./build/quicftpclient localhost:443 upload test_5mb.bin
Uploaded test_5mb.bin (5242880 bytes)

$ mv test_5mb.bin test_5mb_orig.bin
$ ./build/quicftpclient localhost:443 download test_5mb.bin
Downloaded test_5mb.bin (5242880 bytes)

$ md5sum test_5mb_orig.bin test_5mb.bin
00950ebd942cb1ea79c576432b4fd7c1  test_5mb_orig.bin
00950ebd942cb1ea79c576432b4fd7c1  test_5mb.bin
```

### C++ API

```cpp
#include "quicftp_client.h"

quicftp::Client client;

// Connect to server
if (client.connect("https://localhost:443")) {
    // Authenticate (Client Cert - optional)
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

### Error Handling
The client now returns structured errors with HTTP status codes:
```
$ ./build/quicftpclient localhost:443 download nonexistent_file.xyz
Error: Download failed with HTTP 404 (HTTP 404)

$ ./build/quicftpclient localhost:443 upload /nonexistent/path/file.txt
Error: Could not open local file: /nonexistent/path/file.txt
```

Partial batch results report per-file status:
```
$ ./build/quicftpclient localhost:443 download existing.txt missing.xyz other.txt
Downloaded existing.txt (11 bytes)
Error [missing.xyz]: Download failed with HTTP 404 for missing.xyz
Downloaded other.txt (13 bytes)
2/3 files downloaded successfully
```

### Progress Reporting
```
$ ./build/quicftpclient --progress localhost:443 upload large_file.bin
  large_file.bin: 51% (262144/512000 bytes)
  ...
  large_file.bin: 100% (512000/512000 bytes)
Uploaded large_file.bin (512000 bytes)
```

## Testing

### Unit Tests (31 tests)
```bash
cd build
cmake -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTING=ON ..
make -j$(nproc)
ctest --output-on-failure
```
```
100% tests passed, 0 tests failed out of 31
Total Test time (real) =   0.14 sec
```

### Integration Tests (8 tests)
Requires Docker for the Caddy server:
```bash
# Build server image first
docker build -t quicftp-caddy ./docker/caddy

# Run integration tests
bash scripts/integration_test.sh
```
```
=== Quicftp Integration Tests ===
PASS: Single file upload
PASS: Single file download (content verified)
PASS: Parallel upload (3 files)
PASS: Parallel download (3 files)
PASS: Large file transfer (checksum: ef45f34c91a6deb7bca84b9ec5985b2f)
PASS: 404 error handling (no partial file)
PASS: CLI --help
PASS: CLI --version
=== Results: 8 passed, 0 failed ===
```

## Architecture

The project uses **libcurl** (built with HTTP/3 support via ngtcp2 + nghttp3 + OpenSSL) as the client transport layer, and **Caddy** in a Docker container as the QUIC-capable server. This replaced an earlier custom ngtcp2 QUIC stack that was complex and hard to maintain.

```
┌─────────────────────────┐      ┌─────────────────────────┐
│        Client           │      │         Server          │
│ (quicftp::Client + CLI) │      │   (Caddy Container)     │
│           ▼             │      │            ▼            │
│        libcurl          │◄────►│         WebDAV          │
│ (HTTP/3 + ngtcp2 + SSL) │ QUIC │   (HTTP/3 + TLS)        │
└─────────────────────────┘      └─────────────────────────┘
```

**Key design choices:**
- **Client**: `quicftp::Client` wraps libcurl, using `curl_easy` for single transfers and `curl_multi` for parallel transfers. All QUIC/TLS handshake, stream management, and connection logic is handled by libcurl.
- **Server**: Caddy with the WebDAV module handles file uploads (PUT) and downloads (GET) over HTTP/3. Runs in Docker for easy deployment.
- **Protocol**: HTTP/3 over QUIC (UDP). Forced via `CURLOPT_HTTP_VERSION = CURL_HTTP_VERSION_3`.
- **Authentication**: Client TLS certificates (optional). TLS verification is currently disabled for development (Phase 2 will fix this).

## Phase 2 Status

Phase 2 brought the project from "working prototype" to "production-ready." The full proposal is at [`openspec/changes/add-phase2-production-ready/proposal.md`](openspec/changes/add-phase2-production-ready/proposal.md). All tasks tracked in Beads under epic `workspace-n49`.

| Priority | Area | Status | What Changed |
|----------|------|--------|-------------|
| **P0** | Security | **Done** | TLS verification configurable via `set_ca_cert()` / `set_insecure()`. CLI `--ca-cert` and `--insecure` flags. |
| **P1** | Testing | **Done** | 31 unit tests (GoogleTest), 8 integration tests (Docker), GitHub Actions CI/CD pipeline. |
| **P1** | Error Handling | **Done** | `TransferResult` with ErrorCode enum + HTTP status. `BatchTransferResult` for parallel ops. Legacy bool API preserved. |
| **P2** | Progress | **Done** | `ProgressCallback` wired to `CURLOPT_XFERINFOFUNCTION`. Per-file tracking. CLI `--progress` flag. |
| **P2** | CLI | **Done** | `--help`, `--version`, `--verbose`, `--progress`, `--insecure`, `--ca-cert`, `--cert`, `--key` flags. Structured error output. |
| **P3** | Bandwidth | **Done** | `set_bandwidth_limit(upload_bps, download_bps)` via `CURLOPT_MAX_SEND_SPEED_LARGE`. |

**Note:** All Phase 2 features build on libcurl -- no custom QUIC implementation. All features leverage libcurl's `CURLOPT_*` API.

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
