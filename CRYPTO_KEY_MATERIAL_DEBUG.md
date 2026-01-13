# Crypto Key Material Debug Notes

## Test Results

**Test Date**: $(date)
**Status**: ❌ FAILING - Assertion still occurs

## Error
```
quicftpclient: ngtcp2_conn.c:2467: conn_write_handshake_pkt: Assertion `conn->in_pktns->crypto.tx.ckm' failed.
```

## What We've Tried

1. ✅ **Set up SSL_set_app_data with ngtcp2_crypto_conn_ref**
   - Added `conn_ref_` member variable
   - Set up `get_conn` callback
   - Called `SSL_set_app_data(ssl_, &conn_ref_)` before connection creation
   - Result: Still assertion

2. ❌ **Used ngtcp2_crypto_client_initial_cb helper**
   - Tried calling `ngtcp2_crypto_client_initial_cb(conn, user_data)` in client_initial callback
   - Result: Segfault

3. ✅ **Manual TLS handle setting**
   - Set `ngtcp2_conn_set_tls_native_handle(conn, ossl_ctx_)` in client_initial callback
   - Result: No segfault, but assertion still occurs

## Current State

- `SSL_set_app_data` is set up with `conn_ref_`
- `conn_ref_.get_conn` callback returns `self->conn_`
- `conn_` is stored in `client_initial` callback
- TLS handle is set manually in `client_initial` callback
- Keys are NOT being derived (hence the assertion)

## Next Steps

Need to investigate:
1. How does the server derive keys? (It doesn't set SSL_set_app_data)
2. What is the exact sequence of operations needed for key derivation?
3. Are keys derived in a different callback (e.g., recv_crypto_data)?
4. Do we need to manually call key derivation functions?

## Research Needed

- Find working ngtcp2 client examples
- Understand the exact key derivation flow
- Compare with server implementation (which works)
