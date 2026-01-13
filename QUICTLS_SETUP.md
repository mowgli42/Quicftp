# QuicTLS Setup Guide

## Overview

This project now uses **QuicTLS** instead of OpenSSL for QUIC/TLS handshake support. QuicTLS is an OpenSSL fork that provides native QUIC support.

## Installation

QuicTLS must be installed before building this project. Follow these steps:

### 1. Install QuicTLS

#### Option A: Build from Source (Recommended)

```bash
# Clone QuicTLS repository
git clone --depth 1 -b openssl-3.0.0+quic https://github.com/quictls/openssl
cd openssl

# Configure and build
./config enable-tls1_3 --prefix=/usr/local
make -j$(nproc)
sudo make install

# Update library cache
sudo ldconfig
```

#### Option B: Package Manager (if available)

Some distributions provide QuicTLS packages:

**Arch Linux (AUR)**:
```bash
yay -S quictls
```

**Other distributions**: Check your package manager for `quictls` or `openssl-quic` packages.

### 2. Build ngtcp2 with QuicTLS Support

After installing QuicTLS, rebuild ngtcp2 with QuicTLS support:

```bash
# If ngtcp2 is built from source:
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./configure --with-openssl=/usr/local
make
sudo make install
```

Or ensure your distribution's ngtcp2 package includes QuicTLS support.

### 3. Verify Installation

Check that QuicTLS libraries are available:

```bash
# Check for QuicTLS crypto library
ldconfig -p | grep quictls

# Check for ngtcp2 crypto quictls library
ldconfig -p | grep ngtcp2_crypto_quictls
```

### 4. Build Project

The CMake build system will automatically detect and use QuicTLS if available:

```bash
cd /path/to/Quicftp
mkdir -p build
cd build
cmake ..
make
```

## Differences from OpenSSL

### Key Changes

1. **Context-Level Configuration**: QuicTLS uses context-level configuration:
   ```cpp
   ngtcp2_crypto_quictls_configure_client_context(ssl_ctx_);  // Context-level
   ```
   Instead of session-level:
   ```cpp
   ngtcp2_crypto_ossl_configure_client_session(ssl_);  // Session-level
   ```

2. **No Crypto Context**: QuicTLS doesn't require creating a separate `ngtcp2_crypto_ossl_ctx` object. The `SSL*` object is used directly.

3. **Direct SSL Handle**: Set TLS native handle directly with `SSL*`:
   ```cpp
   ngtcp2_conn_set_tls_native_handle(conn_, ssl_);  // Use SSL*, not ossl_ctx*
   ```

## Fallback to OpenSSL

If QuicTLS is not available, the build system will attempt to use OpenSSL, but you may encounter the crypto key material assertion issue. QuicTLS is strongly recommended for QUIC support.

## Troubleshooting

### Library Not Found

If you get "library not found" errors:

1. Verify QuicTLS is installed:
   ```bash
   ls /usr/local/lib/libssl* /usr/local/lib/libcrypto*
   ```

2. Update library cache:
   ```bash
   sudo ldconfig
   ```

3. Check PKG_CONFIG_PATH:
   ```bash
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   ```

### ngtcp2 Not Built with QuicTLS

If ngtcp2 was built with OpenSSL only, you'll need to rebuild it with QuicTLS support or use a distribution package that includes QuicTLS support.

## References

- QuicTLS: https://github.com/quictls/openssl
- ngtcp2: https://github.com/ngtcp2/ngtcp2
- ngtcp2 Examples: https://github.com/ngtcp2/ngtcp2/tree/main/examples

