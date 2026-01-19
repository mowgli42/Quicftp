#!/bin/bash
set -e

# Directory setup
WORK_DIR="$(pwd)/deps"
INSTALL_DIR="$(pwd)/deps/dist"
mkdir -p "$WORK_DIR"
mkdir -p "$INSTALL_DIR"

# Versions
OPENSSL_VERSION="3.2.0"
NGHTTP3_VERSION="1.1.0"
NGTCP2_VERSION="1.2.0"
CURL_VERSION="8.5.0"

echo "Building dependencies in $WORK_DIR"
echo "Installing to $INSTALL_DIR"

# 1. Build OpenSSL (Quic-enabled via standard 3.2+ or fork if needed, but standard 3.2 has some quic support, 
# strictly speaking ngtcp2 usually wants quictls/openssl for full support, let's use quictls)
# Using quictls/openssl for guaranteed compatibility
if [ ! -d "$WORK_DIR/openssl" ]; then
    echo "Cloning OpenSSL (quictls)..."
    git clone --depth 1 -b openssl-3.1.4+quic https://github.com/quictls/openssl.git "$WORK_DIR/openssl"
    cd "$WORK_DIR/openssl"
    ./config --prefix="$INSTALL_DIR" --libdir=lib
    make -j$(nproc)
    make install_sw
fi

# 2. Build nghttp3
if [ ! -d "$WORK_DIR/nghttp3" ]; then
    echo "Cloning nghttp3..."
    git clone --depth 1 -b v$NGHTTP3_VERSION https://github.com/ngtcp2/nghttp3.git "$WORK_DIR/nghttp3"
    cd "$WORK_DIR/nghttp3"
    autoreconf -i
    ./configure --prefix="$INSTALL_DIR" --enable-lib-only
    make -j$(nproc)
    make install
fi

# 3. Build ngtcp2
if [ ! -d "$WORK_DIR/ngtcp2" ]; then
    echo "Cloning ngtcp2..."
    git clone --depth 1 -b v$NGTCP2_VERSION https://github.com/ngtcp2/ngtcp2.git "$WORK_DIR/ngtcp2"
    cd "$WORK_DIR/ngtcp2"
    autoreconf -i
    ./configure PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig" \
        --prefix="$INSTALL_DIR" \
        --enable-lib-only \
        --with-openssl
    make -j$(nproc)
    make install
fi

# 4. Build curl
if [ ! -d "$WORK_DIR/curl" ]; then
    echo "Cloning curl..."
    git clone --depth 1 -b curl-8_5_0 https://github.com/curl/curl.git "$WORK_DIR/curl"
    cd "$WORK_DIR/curl"
    autoreconf -fi
    ./configure PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig" \
        --prefix="$INSTALL_DIR" \
        --with-openssl="$INSTALL_DIR" \
        --with-nghttp3="$INSTALL_DIR" \
        --with-ngtcp2="$INSTALL_DIR" \
        --enable-http3
    make -j$(nproc)
    make install
fi

echo "Build complete. Libs installed to $INSTALL_DIR"
