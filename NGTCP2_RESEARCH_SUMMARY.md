# ngtcp2 Client Implementation Research Summary

## Web Search Findings

Based on ngtcp2 documentation and examples search, here are the key findings:

### 1. Order of Operations (CORRECTED)
The proper order is:
1. **Configure SSL session** (`ngtcp2_crypto_ossl_configure_client_session`)
2. **Create crypto context** (`ngtcp2_crypto_ossl_ctx_new`)
3. **Set SSL_set_app_data** with `ngtcp2_crypto_conn_ref`
4. **Create connection** (calls `client_initial` callback)
5. **Set TLS native handle** (in `client_initial` callback)

**FIXED**: We were creating crypto context before configuring SSL session - this has been corrected.

### 2. Key Derivation Process

Keys are NOT derived in `client_initial` callback:
- `client_initial` callback sets up the connection reference
- Keys are derived during the TLS handshake
- Key derivation happens via `recv_crypto_data` callback (`ngtcp2_crypto_recv_crypto_data_cb`)
- The assertion `conn->in_pktns->crypto.tx.ckm` suggests handshake keys are needed but not derived

### 3. ngtcp2_crypto_client_initial_cb Helper

The helper function:
- Can be passed directly to `callbacks.client_initial`
- Requires `SSL_set_app_data` to be set up BEFORE connection creation
- Derives and installs initial crypto keys
- **Issue**: Causes segfault in our implementation (user_data mismatch?)

### 4. Current Status

**What we've done:**
- ✅ Fixed order: configure SSL session before creating crypto context
- ✅ Set up `SSL_set_app_data` with `ngtcp2_crypto_conn_ref`
- ✅ Set TLS native handle in `client_initial` callback
- ✅ Store connection pointer in callback

**What's still failing:**
- ❌ Assertion: `conn->in_pktns->crypto.tx.ckm` failed (handshake keys not derived)
- ❌ Helper function causes segfault

### 5. Key Insight

The assertion occurs when trying to write handshake packets. This suggests:
- Initial packets use initial keys (should work)
- Handshake packets need handshake keys (not derived yet)
- Keys are derived during TLS handshake, which requires crypto data exchange
- We may be trying to send handshake packets before keys are derived

### 6. Next Investigation Needed

1. When exactly are handshake keys derived?
2. What triggers key derivation in the handshake process?
3. Do we need to wait for server response before sending handshake packets?
4. Is there a working ngtcp2 client example we can study?

## Conclusion

The crypto key material issue is complex and requires deeper understanding of:
- QUIC handshake sequence
- When keys are derived during handshake
- Proper use of ngtcp2 crypto callbacks

Current implementation follows documentation order but keys still aren't derived. This suggests either:
1. Missing step in key derivation process
2. Incorrect callback implementation
3. Need to wait for handshake progress before sending packets

