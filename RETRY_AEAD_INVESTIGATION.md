# Retry AEAD Assertion Investigation

## Problem

```
ngtcp2_conn_set_retry_aead: Assertion '!conn->crypto.retry_aead_ctx.native_handle' failed.
```

This assertion occurs when `ngtcp2_crypto_client_initial_cb` helper function tries to set retry_aead, but it's already been set.

## Analysis

### What is retry_aead?

- Retry AEAD is used by clients to verify the integrity of Retry packets from servers
- Must be set before connection creation or during `client_initial` callback
- Uses `AEAD_AES_128_GCM` cipher with `NGTCP2_RETRY_KEY`

### When is it set?

1. **By `ngtcp2_crypto_client_initial_cb` helper**: This helper function sets retry_aead internally
2. **Manually**: Can be set before `ngtcp2_conn_client_new` if needed

### Current Code Flow

1. Set `SSL_set_app_data(ssl_, &conn_ref_)` with `get_conn` callback
2. Call `ngtcp2_conn_client_new()` which invokes `client_initial` callback
3. `ngtcp2_crypto_client_initial_cb` helper is called
4. Helper tries to set retry_aead → **ASSERTION FAILS**

### Possible Causes

1. **Retry AEAD already set**: Something is setting it before the helper
2. **Helper called multiple times**: Helper is invoked twice somehow
3. **OpenSSL variant issue**: OpenSSL variant might handle retry_aead differently than QuicTLS
4. **TLS native handle conflict**: Setting TLS handle might trigger retry_aead setup

### Example Pattern (simpleclient.c)

```c
// Before ngtcp2_conn_client_new:
c->conn_ref.get_conn = get_conn;  // Function pointer
c->conn_ref.user_data = c;
SSL_set_app_data(c->ssl, &c->conn_ref);

// Then:
ngtcp2_conn_client_new(&c->conn, ...);  // client_initial callback invoked here

// After:
ngtcp2_conn_set_tls_native_handle(c->conn, c->ssl);
```

### Our Code Pattern

```cpp
// Before ngtcp2_conn_client_new:
conn_ref_.get_conn = [](ngtcp2_crypto_conn_ref *ref) -> ngtcp2_conn* {
  QuicClientWrapper* self = static_cast<QuicClientWrapper*>(ref->user_data);
  return self->conn_;  // conn_ is NULL at this point, but lambda is only called later
};
conn_ref_.user_data = this;
SSL_set_app_data(ssl_, &conn_ref_);

// Then:
ngtcp2_conn_client_new(&conn_, ...);  // client_initial callback invoked here

// After:
ngtcp2_conn_set_tls_native_handle(conn_, ssl_);
```

## Differences

1. **get_conn**: Example uses function pointer, we use lambda (should be equivalent)
2. **Timing**: Both set `get_conn` before connection creation
3. **TLS handle**: Both set after connection creation

## Hypothesis

The assertion might be caused by:
- OpenSSL variant (`ngtcp2_crypto_ossl`) handling retry_aead differently than QuicTLS variant
- The helper function expecting QuicTLS-specific setup
- Some initialization happening twice

## Next Steps

1. Check if OpenSSL variant requires different retry_aead handling
2. Verify if `ngtcp2_crypto_client_initial_cb` works with OpenSSL variant
3. Consider implementing custom `client_initial` callback instead of using helper
4. Check if retry_aead needs to be set manually for OpenSSL variant

