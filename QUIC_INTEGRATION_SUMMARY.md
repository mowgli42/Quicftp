# QUIC Integration Research Summary

## Completed Research Tasks

### 1. ngtcp2 Example Code Research ✅
**Status**: Completed
**Findings**:
- No examples found in system directories (`/usr/share/doc/ngtcp2`)
- ngtcp2 structure definitions found in headers:
  - `ngtcp2_crypto_conn_ref` structure defined in `/usr/include/ngtcp2/ngtcp2_crypto.h`
  - Structure contains `get_conn` callback and `user_data` pointer
  - Required for `SSL_set_app_data` setup

### 2. Crypto Key Derivation Research ✅
**Status**: Completed  
**Findings**:
- `ngtcp2_crypto_derive_and_install_tx_key` function handles key derivation
- Keys should be derived automatically if setup is correct
- Manual key derivation may be needed if crypto helpers aren't used
- Documentation found in ngtcp2 headers

### 3. Callback Order Research ✅
**Status**: Completed
**Findings**:
- `client_initial` callback is called during `ngtcp2_conn_client_new`
- Connection pointer is passed to `client_initial` callback
- `ngtcp2_crypto_client_initial_cb` helper requires SSL_set_app_data setup
- Callback sequence: connection creation → client_initial → key derivation

### 4. Architecture Evaluation ✅
**Status**: Completed
**Recommendation**: Continue with Client-Server FTP Model
- See `ARCHITECTURE_EVALUATION.md` for full analysis
- Current model is appropriate for project scope
- P2P can be evaluated as future enhancement

## Current Issue: Crypto Key Material Assertion

### Problem
```
conn_write_handshake_pkt: Assertion 'conn->in_pktns->crypto.tx.ckm' failed
```

### Root Cause Analysis
1. TLS handle is set manually in `client_initial` callback
2. Crypto key material (ckm) is not initialized
3. Keys should be derived during handshake, but derivation isn't happening

### Challenges Identified
1. **SSL_set_app_data Setup**: Requires `ngtcp2_crypto_conn_ref` structure
   - Structure needs `get_conn` callback that returns connection pointer
   - Connection doesn't exist when SSL is created (chicken-and-egg problem)
   - Connection is created in `ngtcp2_conn_client_new`, which calls `client_initial`

2. **Crypto Helper Usage**: `ngtcp2_crypto_client_initial_cb` helper
   - Requires SSL_set_app_data to be set up
   - Previous attempt: segfaulted (likely due to missing SSL_set_app_data)
   - Helper should handle key derivation automatically

3. **Timing Issue**: Connection pointer availability
   - Connection pointer passed to `client_initial` callback
   - But `conn_` member variable not set until after `ngtcp2_conn_client_new` returns
   - Need to set SSL_set_app_data before connection creation

### Potential Solutions

#### Option 1: Set up conn_ref before connection creation
- Store `ngtcp2_crypto_conn_ref` as member variable
- Set `get_conn` callback to access `this->conn_` via user_data
- Set `SSL_set_app_data` before `ngtcp2_conn_client_new`
- Issue: `conn_` not set during `client_initial` callback

#### Option 2: Use connection pointer in callback
- Store connection pointer passed to `client_initial` in conn_ref
- Set up conn_ref structure in `client_initial` callback
- Issue: SSL_set_app_data must be set BEFORE connection creation

#### Option 3: Two-phase setup
- Set up conn_ref with placeholder before connection creation
- Update conn_ref in `client_initial` callback with actual connection
- Issue: Timing and synchronization challenges

### Next Steps for Crypto Key Material Fix

1. **Further Research Needed**:
   - Find working ngtcp2 client examples (GitHub, official docs)
   - Understand exact sequence of SSL_set_app_data setup
   - Study ngtcp2 source code if available

2. **Implementation Strategy**:
   - Try Option 1 with careful handling of connection pointer
   - Consider storing connection pointer in user_data of conn_ref
   - Test with ngtcp2_crypto_client_initial_cb helper again after SSL_set_app_data setup

3. **Alternative Approach**:
   - If crypto helpers prove problematic, implement manual key derivation
   - Use `ngtcp2_crypto_derive_and_install_tx_key` directly
   - Requires deeper understanding of QUIC handshake stages

## Documentation Created

1. **RESEARCH_NOTES.md**: Technical research findings on ngtcp2 structures and APIs
2. **ARCHITECTURE_EVALUATION.md**: Complete analysis of FTP vs P2P architectures
3. **QUIC_INTEGRATION_SUMMARY.md**: This summary document

## Recommendations

1. **Immediate**: Continue research on crypto key material fix
   - Priority: Find working ngtcp2 client examples
   - Focus: Understanding SSL_set_app_data setup pattern

2. **Short-term**: Complete core functionality
   - Fix crypto key material assertion (blocking issue)
   - Complete QUIC handshake implementation
   - Implement file transfer over QUIC streams

3. **Long-term**: Evaluate enhancements
   - Consider P2P architecture after core functionality is stable
   - Add advanced QUIC features (0-RTT, connection migration, etc.)

## Research Gaps

1. **Missing Examples**: No working ngtcp2 client examples found in system
2. **Timing Details**: Exact sequence of SSL_set_app_data setup unclear
3. **Connection Pointer Handling**: Best way to handle connection pointer in conn_ref
4. **Key Derivation Timing**: When exactly keys should be derived

## Resources Found

- ngtcp2 header files: `/usr/include/ngtcp2/`
- Structure definitions: `ngtcp2_crypto_conn_ref` in `ngtcp2_crypto.h`
- API documentation: In header files
- Web resources: Various QUIC file transfer examples (quic-send, qcp, etc.)

