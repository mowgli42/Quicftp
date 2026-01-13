# Handshake Optimization Plan

## Problem Analysis

Based on our research, the crypto key material assertion occurs because:
1. `ngtcp2_conn_write_pkt()` is called immediately after connection creation
2. ngtcp2 tries to send handshake packets (after initial packet)
3. Handshake keys haven't been derived yet (they're derived during TLS handshake)
4. Assertion fails: `conn->in_pktns->crypto.tx.ckm` not available

## Key Insight

**Initial packets use pre-shared keys** - these work immediately ✅  
**Handshake packets need derived keys** - these are only available after TLS handshake progresses ❌

## Solution Strategy

### Option 1: Use ngtcp2_crypto_read_write_crypto_data (Recommended)

Drive the TLS handshake forward before sending packets:

1. Create connection
2. Use `ngtcp2_crypto_read_write_crypto_data()` to drive TLS handshake
3. This will process crypto data and derive keys
4. Then send packets (keys will be available)

### Option 2: Wait for Server Response First

1. Send initial packet only
2. Wait for server response
3. Process incoming packets (drives handshake)
4. Keys get derived via `update_key` callback
5. Then send handshake packets (keys available)

### Option 3: Use ngtcp2_crypto_client_initial_cb Properly

The helper function should work if:
- SSL_set_app_data is set up correctly ✅ (we have this)
- Connection pointer is available when helper is called
- Helper can access SSL object via conn_ref

**Issue**: Helper causes segfault - needs investigation

## Recommended Approach

**Use Option 1**: Drive TLS handshake with `ngtcp2_crypto_read_write_crypto_data`

This function:
- Reads crypto data at specified encryption level
- Feeds outgoing crypto data to connection
- Drives the TLS handshake forward
- Enables key derivation before sending packets

## Implementation Steps

1. After connection creation, call `ngtcp2_crypto_read_write_crypto_data`
2. This will process any pending crypto data and drive handshake
3. Keys will be derived via `update_key` callback
4. Then `ngtcp2_conn_write_pkt` can send handshake packets safely

