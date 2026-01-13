# Retry AEAD Assertion Analysis

## Current Status

The assertion `ngtcp2_conn_set_retry_aead: Assertion '!conn->crypto.retry_aead_ctx.native_handle' failed` persists even when using the helper function `ngtcp2_crypto_client_initial_cb` for both QuicTLS and OpenSSL variants.

## Key Finding

The helper function `ngtcp2_crypto_client_initial_cb` is supposed to:
1. Set TLS native handle
2. Set retry_aead
3. Install initial keys

But the assertion suggests retry_aead is **already set** when the helper tries to set it.

## Possible Causes

1. **Helper called multiple times**: The callback might be invoked twice
2. **Pre-initialization**: Something sets retry_aead before `ngtcp2_conn_client_new`
3. **OpenSSL variant incompatibility**: The helper might not work correctly with OpenSSL variant
4. **Connection state**: The connection might be in a state where retry_aead is already initialized

## Next Investigation Steps

1. Check if `client_initial` callback is being called multiple times
2. Verify if retry_aead needs to be set BEFORE `ngtcp2_conn_client_new` for OpenSSL variant
3. Check ngtcp2 source code to understand when retry_aead is initialized
4. Consider if we need to NOT use the helper and implement manual setup

## Current Code Pattern

```cpp
// Before connection creation:
conn_ref_.get_conn = [](...) -> ngtcp2_conn* { return self->conn_; };
conn_ref_.user_data = this;
SSL_set_app_data(ssl_, &conn_ref_);

// Connection creation:
callbacks.client_initial = ngtcp2_crypto_client_initial_cb;
ngtcp2_conn_client_new(&conn_, ...);  // Helper called here

// After connection creation:
ngtcp2_conn_set_tls_native_handle(conn_, ssl_);
```

This matches the example pattern, but assertion still occurs.

