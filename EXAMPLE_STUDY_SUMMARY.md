# ngtcp2 Example Study Summary

## Findings from simpleclient.c Example

1. **Uses `ngtcp2_crypto_client_initial_cb` directly** - Line 288
2. **Sets TLS native handle with `SSL*`, NOT `ossl_ctx*`** - Line 339
3. **Sets `conn_ref.get_conn` AFTER connection creation** - Line 625
4. **Sets `SSL_set_app_data` BEFORE connection creation** - Line 186

## Key Differences from Our Code

### Example Uses QuicTLS, We Use OpenSSL
- Example: `ngtcp2_crypto_quictls_configure_client_context(c->ssl_ctx)`
- Our code: `ngtcp2_crypto_ossl_configure_client_session(ssl_)`

### Order of Operations

**Example (simpleclient.c)**:
1. `SSL_set_app_data(c->ssl, &c->conn_ref)` - BEFORE connection (line 186)
2. `ngtcp2_conn_client_new(...)` - Creates connection, calls `client_initial` callback (line 332)
3. `ngtcp2_conn_set_tls_native_handle(c->conn, c->ssl)` - AFTER connection (line 339)
4. `c->conn_ref.get_conn = get_conn` - AFTER connection (line 625)

**Our Current Code**:
1. Set up `conn_ref_.get_conn` lambda - BEFORE connection
2. `SSL_set_app_data(ssl_, &conn_ref_)` - BEFORE connection
3. `ngtcp2_conn_client_new(...)` - Creates connection, calls `client_initial` callback
4. (Helper function should handle TLS setup)

## Issues Encountered

1. **Segfault with helper function** - `ngtcp2_crypto_client_initial_cb` causes segfault
2. **Assertion with manual setup** - Crypto key material assertion persists
3. **Different TLS backend** - Example uses QuicTLS, we use OpenSSL

## Next Steps

The example code uses QuicTLS, which may have different behavior than OpenSSL. We may need to:
1. Study OpenSSL-specific examples if available
2. Understand what `ngtcp2_crypto_client_initial_cb` expects
3. Or find a different approach that works with OpenSSL

