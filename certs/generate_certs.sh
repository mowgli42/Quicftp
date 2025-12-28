#!/bin/bash
# Generate test certificates for Quicftp development

set -e

CERT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$CERT_DIR"

echo "Generating test certificates for Quicftp..."

# Generate CA private key
openssl genrsa -out ca-key.pem 2048

# Generate CA certificate
openssl req -new -x509 -days 365 -key ca-key.pem -out ca-cert.pem \
    -subj "/C=US/ST=State/L=City/O=Quicftp/CN=Quicftp CA"

# Generate server private key
openssl genrsa -out server-key.pem 2048

# Generate server certificate request
openssl req -new -key server-key.pem -out server.csr \
    -subj "/C=US/ST=State/L=City/O=Quicftp/CN=localhost"

# Sign server certificate with CA
openssl x509 -req -days 365 -in server.csr -CA ca-cert.pem -CAkey ca-key.pem \
    -CAcreateserial -out server-cert.pem

# Generate client private key
openssl genrsa -out client-key.pem 2048

# Generate client certificate request
openssl req -new -key client-key.pem -out client.csr \
    -subj "/C=US/ST=State/L=City/O=Quicftp/CN=client"

# Sign client certificate with CA
openssl x509 -req -days 365 -in client.csr -CA ca-cert.pem -CAkey ca-key.pem \
    -CAcreateserial -out client-cert.pem

# Clean up
rm -f server.csr client.csr ca-cert.srl

echo "Certificates generated:"
echo "  CA: ca-cert.pem, ca-key.pem"
echo "  Server: server-cert.pem, server-key.pem"
echo "  Client: client-cert.pem, client-key.pem"

