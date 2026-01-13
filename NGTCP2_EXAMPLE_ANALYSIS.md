# ngtcp2 Example Analysis

## Key Findings from simpleclient.c Example

### Critical Difference #1: TLS Native Handle

**Example Code (simpleclient.c:340)**:
```c
ngtcp2_conn_set_tls_native_handle(c->conn, c->ssl);  // Uses SSL*, NOT ossl_ctx!
```

**Our Code**:
```cpp
ngtcp2_conn_set_tls_native_handle(conn, self->ossl_ctx_);  // Uses ossl_ctx
```

**This is the issue!** We're passing `ossl_ctx_` but the example passes `ssl`.

### Critical Difference #2: client_initial Callback

**Example Code**:
```c
callbacks.client_initial = ngtcp2_crypto_client_initial_cb;  // Direct assignment
```

**Our Code**:
```cpp
callbacks.client_initial = [](ngtcp2_conn *conn, void *user_data) -> int {
  // Manual implementation
  ngtcp2_conn_set_tls_native_handle(conn, self->ossl_ctx_);  // WRONG!
  return 0;
};
```

### Critical Difference #3: Order of Operations

**Example Code Order**:
1. `SSL_set_app_data(c->ssl, &c->conn_ref)` (line 186) - Before connection creation
2. `ngtcp2_conn_client_new(...)` (line 332) - Creates connection
3. `ngtcp2_conn_set_tls_native_handle(c->conn, c->ssl)` (line 340) - After connection creation, uses SSL*

**Our Code Order**:
1. Set up `conn_ref_.get_conn` lambda
2. `SSL_set_app_data(ssl_, &conn_ref_)`
3. `ngtcp2_conn_client_new(...)` - client_initial callback tries to set ossl_ctx
4. Connection created

### Key Insight

The example uses **QuicTLS** (`ngtcp2_crypto_quictls_configure_client_context`), but we're using **OpenSSL** (`ngtcp2_crypto_ossl_configure_client_session`).

However, the critical point is:
- **TLS native handle should be `SSL*`, NOT `ossl_ctx*`**
- The helper function `ngtcp2_crypto_client_initial_cb` should handle everything
- `ngtcp2_conn_set_tls_native_handle` should be called with `ssl_`, not `ossl_ctx_`

## Solution

1. Use `ngtcp2_crypto_client_initial_cb` directly as the callback
2. Call `ngtcp2_conn_set_tls_native_handle(conn, ssl_)` after connection creation (not in client_initial)
3. OR: Remove the manual `ngtcp2_conn_set_tls_native_handle` call if using the helper

