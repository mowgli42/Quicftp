# Project Context

## Purpose
Quicftp is a file transfer client library and CLI tool that enables file transfer over the QUIC protocol. The project provides:
- A C++ client library (`quicftp::Client`) for programmatic file transfer operations
- A command-line interface for uploading and downloading files
- Certificate-based authentication for secure connections
- Support for multiple file transfers in a single session

## Tech Stack
- **C++** (C++11 or later) - Primary language
- **QUIC Protocol** - Network transport layer for file transfer
- **Standard C++ Library** - Core dependencies (iostream, string, vector)
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

## Domain Context
- **QUIC Protocol**: Modern transport protocol providing low-latency, multiplexed connections over UDP
- **File Transfer**: Upload and download operations over QUIC streams
- **Certificate Authentication**: TLS certificate-based authentication for secure connections
- **CLI Usage**: Command format: `<server> <upload|download> <file1> <file2> ...`

## Important Constraints
- **MIT License**: Project is licensed under MIT License (Copyright 2023 mowgli42)
- **C++ Standard**: Requires C++11 or later for standard library features
- **QUIC Implementation**: Depends on a QUIC library (specific implementation to be determined)
- **Certificate Requirements**: Requires valid TLS certificates for authentication

## External Dependencies
- **QUIC Library**: Specific QUIC implementation library (to be determined/selected)
- **TLS/Crypto Library**: For certificate validation and encryption (likely provided by QUIC library)
- **Standard C++ Library**: Core language features and containers
