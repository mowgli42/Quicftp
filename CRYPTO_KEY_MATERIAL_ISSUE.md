# Crypto Key Material Issue - Documentation

## Status

**Current State**: ❌ **BLOCKING ISSUE** - Not resolved  
**Last Updated**: 2025-01-12  
**Priority**: CRITICAL - Blocks all file transfer functionality

## Problem

### Error Message
```
quicftpclient: ngtcp2_conn.c:2467: conn_write_handshake_pkt: Assertion `conn->in_pktns->crypto.tx.ckm' failed.
```

### What This Means
- `conn->in_pktns->crypto.tx.ckm` = Connection's initial packet number space's crypto transmit key material
- The assertion fails when ngtcp2 tries to write handshake packets
- This indicates that handshake crypto keys have not been derived/installed
- The connection cannot proceed with the TLS handshake without these keys

## Root Cause Analysis

### QUIC/TLS Handshake Sequence
1. **Initial Packets**: Use pre-shared initial keys (stateless, no derivation needed)
2. **Handshake Packets**: Require derived handshake keys (from TLS handshake secrets)
3. **1-RTT Packets**: Require derived 1-RTT keys (after handshake completes)

### Where Keys Are Derived
- Keys are **NOT** derived in the `client_initial` callback
- Keys are derived during the TLS handshake via crypto data exchange
- Key derivation happens through the `recv_crypto_data` callback
- The `client_initial` callback only sets up the connection reference

### The Issue
We're trying to send handshake packets before the TLS handshake has progressed enough to derive handshake keys. The handshake keys are derived from TLS secrets that are exchanged during the crypto data phase of the handshake.

## What We've Tried

### 1. Set up SSL_set_app_data with ngtcp2_crypto_conn_ref ✅
**Implementation**:
- Added `ngtcp2_crypto_conn_ref conn_ref_` member variable
- Configured `get_conn` callback to return connection pointer
- Called `SSL_set_app_data(ssl_, &conn_ref_)` before connection creation

**Result**: ✅ Implemented correctly, but assertion persists

### 2. Fixed Order of Operations ✅
**Original Order** (WRONG):
1. Create crypto context (`ngtcp2_crypto_ossl_ctx_new`)
2. Configure SSL session (`ngtcp2_crypto_ossl_configure_client_session`)

**Corrected Order**:
1. Configure SSL session (`ngtcp2_crypto_ossl_configure_client_session`)
2. Create crypto context (`ngtcp2_crypto_ossl_ctx_new`)
3. Set SSL_set_app_data
4. Create connection
5. Set TLS native handle in `client_initial` callback

**Result**: ✅ Fixed order, but assertion persists

### 3. Used ngtcp2_crypto_client_initial_cb Helper ❌
**Attempt**: Called `ngtcp2_crypto_client_initial_cb(conn, user_data)` in `client_initial` callback

**Result**: ❌ Segmentation fault (possible user_data mismatch or setup issue)

### 4. Manual TLS Handle Setting ✅
**Implementation**: Set `ngtcp2_conn_set_tls_native_handle(conn, ossl_ctx_)` in `client_initial` callback

**Result**: ✅ No segfault, but assertion persists (keys still not derived)

## Current Implementation

### Connection Setup Sequence
```cpp
// 1. Configure SSL session for QUIC
ngtcp2_crypto_ossl_configure_client_session(ssl_);

// 2. Create ngtcp2 crypto context
ngtcp2_crypto_ossl_ctx_new(&ossl_ctx_, ssl_);

// 3. Set up ngtcp2_crypto_conn_ref for SSL_set_app_data
conn_ref_.get_conn = [](ngtcp2_crypto_conn_ref *ref) -> ngtcp2_conn* {
  QuicClientWrapper* self = static_cast<QuicClientWrapper*>(ref->user_data);
  return self->conn_;
};
conn_ref_.user_data = this;
SSL_set_app_data(ssl_, &conn_ref_);

// 4. Set SSL to connect state
SSL_set_connect_state(ssl_);

// 5. Create ngtcp2 connection (calls client_initial callback)
ngtcp2_conn_client_new(&conn_, ...);

// 6. In client_initial callback:
//    - Store connection pointer (self->conn_ = conn)
//    - Set TLS native handle (ngtcp2_conn_set_tls_native_handle(conn, ossl_ctx_))
```

### Callbacks Configuration
- `client_initial`: Sets connection pointer and TLS native handle
- `recv_crypto_data`: Uses `ngtcp2_crypto_recv_crypto_data_cb` helper
- `encrypt/decrypt/hp_mask`: Use ngtcp2 crypto helpers
- Other callbacks: Various custom implementations

## Research Findings

### From ngtcp2 Documentation
1. **SSL_set_app_data is Required**: Must be set before connection creation
2. **Order Matters**: Configure SSL session before creating crypto context
3. **Key Derivation**: Happens during handshake, not in `client_initial` callback
4. **Helper Functions**: `ngtcp2_crypto_client_initial_cb` can be used but requires proper setup

### Key Insights
1. **Initial vs Handshake Keys**: 
   - Initial packets use pre-shared initial keys (work fine)
   - Handshake packets need derived keys (not available yet)
   
2. **Handshake Progress**:
   - TLS handshake must progress via crypto data exchange
   - Keys are derived as secrets become available
   - The `recv_crypto_data` callback processes incoming crypto data
   
3. **Chicken-and-Egg Problem**:
   - Need to send initial packet to start handshake
   - Handshake needs to progress to derive keys
   - But assertion occurs when trying to send handshake packets
   - Suggests handshake isn't progressing properly

## Comparison with Server Implementation

### Server (Working)
- Uses `recv_client_initial` callback (`ngtcp2_crypto_recv_client_initial_cb`)
- Sets TLS native handle **AFTER** connection creation
- Does **NOT** set `SSL_set_app_data` (different flow)
- Works because server responds to client's initial packet

### Client (Not Working)
- Uses `client_initial` callback (must set up connection reference)
- Must set `SSL_set_app_data` BEFORE connection creation
- Sets TLS native handle in `client_initial` callback
- Fails because keys aren't derived before handshake packets are sent

## What's Missing

### Possible Missing Steps
1. **Drive TLS Handshake Forward**: 
   - May need to manually drive TLS handshake before sending packets
   - Use `SSL_do_handshake` or similar to advance TLS state
   
2. **Process Crypto Data First**:
   - May need to process incoming crypto data before sending handshake packets
   - Wait for server response before attempting to send handshake packets
   
3. **Use Different Approach**:
   - May need to use `ngtcp2_crypto_client_initial_cb` helper correctly
   - Requires understanding what `user_data` should be
   
4. **Timing Issue**:
   - May be trying to send handshake packets too early
   - Need to wait for initial handshake exchange to complete

## Handshake Flow Analysis

**See**: `QUIC_HANDSHAKE_FLOW.md` for detailed handshake sequence documentation

### Key Finding

The assertion occurs because:
- We're trying to send handshake packets before handshake keys are derived
- Handshake keys are only derived after server responds and TLS handshake progresses
- `ngtcp2_conn_write_pkt()` tries to send handshake packets immediately
- Keys haven't been derived yet (they're derived during handshake, not in `client_initial`)

### Handshake Sequence

1. **Initial Stage**: Pre-shared initial keys (work immediately) ✅
2. **Handshake Stage**: Derived handshake keys (need TLS handshake progress) ❌
3. **1-RTT Stage**: Derived 1-RTT keys (after handshake completes)

Keys are derived:
- **When**: During TLS handshake via crypto data exchange
- **How**: `update_key` callback receives secrets and derives keys
- **NOT**: In `client_initial` callback (which only sets up connection reference)

## Next Steps for Future Investigation

1. **Find Working Examples**:
   - Search for ngtcp2 client source code examples
   - Study official ngtcp2 examples if available
   - Compare implementation patterns

2. **Understand Packet Sending Sequence**:
   - Does `ngtcp2_conn_write_pkt` try to send handshake packets immediately?
   - Can we control what packet types it sends?
   - Should we only send initial packets first, then wait?

3. **Debug TLS State**:
   - Check TLS handshake state at assertion point
   - Verify crypto data is being processed
   - Check if secrets are available

4. **Alternative Approaches**:
   - Wait for server response before sending handshake packets
   - Only send initial packets initially
   - Drive TLS handshake forward before sending packets

## References

- ngtcp2 Programmer's Guide: https://nghttp2.org/ngtcp2/programmers-guide.html
- ngtcp2 API Documentation: https://nghttp2.org/ngtcp2/
- Research Notes: `RESEARCH_NOTES.md`
- ngtcp2 Examples Research: `NGTCP2_EXAMPLES_RESEARCH.md`
- Research Summary: `NGTCP2_RESEARCH_SUMMARY.md`

## Impact

**Blocks**:
- ✅ Connection establishment
- ✅ File transfer operations
- ✅ All QUIC functionality

**Cannot Proceed With**:
- Stream creation (requires connection)
- File upload/download (requires connection)
- Any QUIC-based operations

## Conclusion

The crypto key material assertion is a complex issue that requires deeper understanding of the QUIC/TLS handshake sequence. The current implementation follows ngtcp2 documentation for setup, but keys are not being derived at the right time. This suggests either:

1. Missing step in driving the TLS handshake forward
2. Incorrect callback implementation
3. Timing issue with when packets are sent
4. Need to use helper functions differently

**Recommendation**: This issue requires additional research into ngtcp2 client implementation patterns, preferably from working examples or the ngtcp2 source code itself.

