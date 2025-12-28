# QUIC Library Selection

## Selected Library: ngtcp2

**Rationale:**
- Pure C library with clean C++ integration
- Actively maintained and well-documented
- Supports all required QUIC features:
  - Stream multiplexing
  - 0-RTT connection resumption
  - Connection migration
  - Flow control and congestion control
  - Path MTU discovery
- Works with OpenSSL or BoringSSL for TLS
- Cross-platform (Linux, macOS, Windows)

## Integration Approach

1. **TLS Backend**: Use OpenSSL (commonly available) or BoringSSL
2. **Build System**: CMake for dependency management
3. **Wrapper**: Create C++ wrapper classes around ngtcp2 C API
4. **Stream Management**: Track streams in C++ containers, use ngtcp2 stream IDs

## Dependencies

- ngtcp2 (QUIC protocol implementation)
- nghttp3 (HTTP/3 layer, optional but useful for stream management)
- OpenSSL or BoringSSL (TLS/crypto)
- C++17 compiler (for filesystem support)

## Alternative Considered

- **msquic**: Good but Windows-focused, less portable
- **quiche**: Rust-based, adds complexity
- **picoquic**: Simpler but less feature-complete

