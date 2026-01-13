# Handshake Optimization Summary

## Current Status

**Problem**: Crypto key material assertion persists
- `ngtcp2_conn_write_pkt()` tries to send handshake packets before keys are derived
- Assertion occurs inside ngtcp2 (can't catch it)
- Keys are only derived during TLS handshake via `update_key` callback

## What We've Learned

1. **Handshake Flow**:
   - Initial packets: Pre-shared keys ✅ (work immediately)
   - Handshake packets: Derived keys ❌ (need TLS handshake progress)
   - 1-RTT packets: Derived keys (after handshake completes)

2. **Key Derivation**:
   - Keys are NOT derived in `client_initial` callback
   - Keys are derived via `update_key` callback during TLS handshake
   - `update_key` is called when TLS secrets become available
   - Keys are installed via `ngtcp2_crypto_derive_and_install_tx_key`

3. **The Issue**:
   - `ngtcp2_conn_write_pkt()` doesn't let us control packet types
   - It sends what ngtcp2 wants to send
   - If ngtcp2 wants handshake packets, it tries immediately
   - But keys aren't available yet → assertion

## Attempted Solutions

1. ✅ Set up `SSL_set_app_data` with `ngtcp2_crypto_conn_ref`
2. ✅ Fixed order of operations
3. ❌ Used `ngtcp2_crypto_client_initial_cb` helper → segfault
4. ✅ Manual TLS handle setting → no segfault, but assertion persists
5. ⚠️ Tried to wait for server response → assertion still occurs

## Root Cause

The assertion happens **inside** `ngtcp2_conn_write_pkt()` when it tries to write handshake packets. We can't prevent this from happening because:
- We can't control what packet types ngtcp2 wants to send
- The assertion is inside ngtcp2 (not our code)
- Keys aren't derived until TLS handshake progresses
- But ngtcp2 tries to send handshake packets before handshake progresses

## Next Steps

The issue requires one of:
1. **Fix in ngtcp2 usage**: Find the correct way to prevent handshake packet sending until keys are derived
2. **Use ngtcp2_crypto_client_initial_cb properly**: Fix the segfault issue
3. **Manual key derivation**: Derive keys manually before calling `ngtcp2_conn_write_pkt`
4. **Different approach**: Use a different ngtcp2 API that doesn't try to send handshake packets immediately

## Recommendation

This is a complex ngtcp2 integration issue that requires:
- Working ngtcp2 client examples to study
- Understanding the exact sequence ngtcp2 expects
- Possibly using different ngtcp2 APIs or patterns

The current implementation follows documentation but still hits the assertion. This suggests either:
- Missing step in the setup
- Incorrect usage pattern
- Need for different API approach

