# ngtcp2 Client Examples Research

## Key Findings from Web Search

### 1. Order of Operations (Critical!)
According to ngtcp2 documentation, the order is:
1. **Configure SSL session** (e.g., `ngtcp2_crypto_ossl_configure_client_session`)
2. **Create ngtcp2_crypto_ossl_ctx** (e.g., `ngtcp2_crypto_ossl_ctx_new`)
3. **Set SSL_set_app_data** with `ngtcp2_crypto_conn_ref` BEFORE connection creation
4. **Set TLS native handle** (via `ngtcp2_conn_set_tls_native_handle`)
5. **Create connection** (which calls `client_initial` callback)

### 2. ngtcp2_crypto_client_initial_cb Usage

The `ngtcp2_crypto_client_initial_cb` function:
- Can be **directly passed** to `callbacks.client_initial` field
- Requires `SSL_set_app_data` to be set up BEFORE connection creation
- Derives and installs initial crypto keys

However, when used directly, it expects the `user_data` parameter to be something that can access the SSL object via `SSL_get_app_data`.

### 3. Key Derivation

Keys are derived:
- During the TLS handshake process
- Via the `recv_crypto_data` callback (which uses `ngtcp2_crypto_recv_crypto_data_cb`)
- NOT directly in the `client_initial` callback

The `client_initial` callback sets up the connection reference, but keys are derived later as crypto data is exchanged.

### 4. Current Implementation Issues

Our current implementation:
- ✅ Sets up SSL_set_app_data (correct)
- ✅ Sets TLS native handle in client_initial callback
- ❌ May have wrong order of operations
- ❌ May not be handling the callback correctly

### 5. Server vs Client Difference

**Server:**
- Uses `recv_client_initial` callback (`ngtcp2_crypto_recv_client_initial_cb`)
- Sets TLS native handle AFTER connection creation
- Does NOT set SSL_set_app_data (different flow)

**Client:**
- Uses `client_initial` callback
- Must set SSL_set_app_data BEFORE connection creation
- TLS native handle should be set... when?

### 6. Recommended Approach

Based on documentation:

1. **Before connection creation:**
   ```cpp
   // Configure SSL session
   ngtcp2_crypto_ossl_configure_client_session(ssl_);
   
   // Create crypto context
   ngtcp2_crypto_ossl_ctx_new(&ossl_ctx_, ssl_);
   
   // Set up conn_ref
   conn_ref_.get_conn = [](ngtcp2_crypto_conn_ref *ref) -> ngtcp2_conn* {
     QuicClientWrapper* self = static_cast<QuicClientWrapper*>(ref->user_data);
     return self->conn_;
   };
   conn_ref_.user_data = this;
   SSL_set_app_data(ssl_, &conn_ref_);
   ```

2. **In client_initial callback:**
   ```cpp
   // Store connection pointer
   self->conn_ = conn;
   
   // Set TLS native handle
   ngtcp2_conn_set_tls_native_handle(conn, self->ossl_ctx_);
   ```

3. **Or use the helper directly:**
   ```cpp
   callbacks.client_initial = ngtcp2_crypto_client_initial_cb;
   ```

## Key Insight

The assertion `conn->in_pktns->crypto.tx.ckm` failed suggests that:
- The TLS native handle may not be set correctly
- OR keys haven't been derived yet (they're derived during handshake, not in client_initial)
- OR the order of operations is wrong

## Next Steps

1. Try using `ngtcp2_crypto_client_initial_cb` directly (but need to figure out user_data)
2. Ensure TLS native handle is set in client_initial BEFORE keys are needed
3. Check if we need to wait for recv_crypto_data callback before sending packets

