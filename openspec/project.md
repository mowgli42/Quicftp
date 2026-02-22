# Project Context

## Purpose
Quicftp is a file transfer client library and CLI tool that enables file transfer over the QUIC protocol. The project provides:
- A C++ client library (`quicftp::Client`) for programmatic file transfer operations
- A command-line interface for uploading and downloading files
- Certificate-based authentication for secure connections
- Support for multiple file transfers in a single session

## Tech Stack
- **C++** (C++17) - Primary language (C++17 required for filesystem support)
- **ngtcp2** - QUIC protocol implementation library
- **ngtcp2_crypto_ossl** - OpenSSL integration for ngtcp2
- **nghttp3** - HTTP/3 layer library (optional, for stream management)
- **OpenSSL** - TLS/crypto backend
- **Standard C++ Library** - Core dependencies (iostream, string, vector, filesystem)
- **TLS/Certificates** - Authentication mechanism (certificate-based)

## Project Conventions

### Code Style
- **Naming Conventions**:
  - Methods: `snake_case` (e.g., `upload_file`, `download_file`, `authenticate`)
  - Classes: `PascalCase` (e.g., `Client`)
  - Namespace: `lowercase` (e.g., `quicftp`)
  - Files: `snake_case` for headers (e.g., `quicftp_client.h`), `kebab-case` for CLI (e.g., `quicftpclient-cli.cc`)
  
- **Header Guards**: Use `#ifndef` / `#define` pattern (e.g., `QUICFTP_CLIENT_H`)
- **Namespace**: All library code should be in the `quicftp` namespace
- **Return Types**: Use `bool` for operations that can succeed/fail, `void` for operations that don't return status
- **Error Handling**: Methods return `false` on failure, allowing callers to check and handle errors

### Architecture Patterns
- **Client-Server Model**: Client connects to a QUIC server for file operations
- **Session-Based**: Connection lifecycle managed through `connect()` and `disconnect()`
- **Authentication Flow**: Certificate-based authentication before file operations
- **Class-Based API**: Core functionality encapsulated in `quicftp::Client` class
- **CLI Separation**: Command-line interface (`quicftpclient-cli.cc`) is separate from library implementation

### Testing Strategy
- Testing approach to be defined as the project develops
- Consider unit tests for client methods
- Integration tests for end-to-end file transfer scenarios
- Certificate validation and error handling tests

### Git Workflow
- Git workflow conventions to be established
- Follow OpenSpec change proposal workflow for significant changes (see `openspec/AGENTS.md`)
- Use Beads (`bd`) for persistent task tracking across sessions (see `AGENTS.md` for session protocol)
- OpenSpec for formal specs/proposals; Beads for day-to-day tasks and progress

## Domain Context
- **QUIC Protocol**: Modern transport protocol providing low-latency, multiplexed connections over UDP
- **File Transfer**: Upload and download operations over QUIC streams
- **Certificate Authentication**: TLS certificate-based authentication for secure connections
- **CLI Usage**: Command format: `<server> <upload|download> <file1> <file2> ...`

## Important Constraints
- **MIT License**: Project is licensed under MIT License (Copyright 2023 mowgli42)
- **C++ Standard**: Requires C++17 for filesystem support and standard library features
- **QUIC Implementation**: Uses ngtcp2 library for QUIC protocol implementation
- **Certificate Requirements**: Requires valid TLS certificates for authentication
- **Build System**: Uses CMake for dependency management and build configuration

## Implementation Status

### Completed Features
- **QUIC/TLS Handshake**: Both client and server implement full QUIC/TLS handshake using ngtcp2
  - Client: Initial packet sending, TLS handshake via ngtcp2 callbacks, packet reception
  - Server: Initial packet reception, connection creation, TLS handshake processing, handshake response
- **QUIC Library Integration**: ngtcp2 integrated with OpenSSL backend (ngtcp2_crypto_ossl)
- **UDP Socket Management**: Non-blocking UDP sockets for QUIC packet transmission
- **Certificate-Based Authentication**: TLS certificate loading and validation infrastructure
- **Connection Management**: Basic connection lifecycle (connect, authenticate, disconnect)

### In Progress
- **File Transfer over QUIC Streams**: Stream creation and data transfer implementation
- **Stream Multiplexing**: Multiple parallel file transfers
- **Error Recovery**: Packet loss handling and retransmission

### Planned Features
- Stream prioritization
- 0-RTT connection resumption
- Connection migration
- Flow control and congestion control
- Path MTU discovery
- Performance monitoring

For detailed library information, see `QUIC_LIBRARY.md`. For feature requirements, see `openspec/specs/`.

## External Dependencies
- **ngtcp2**: QUIC protocol implementation library (C library)
- **ngtcp2_crypto_ossl**: OpenSSL integration layer for ngtcp2
- **nghttp3**: HTTP/3 layer library (optional, useful for stream management)
- **OpenSSL**: TLS/crypto library for certificate validation and encryption
- **Standard C++ Library**: Core language features and containers (C++17)
