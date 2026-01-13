# QUIC/TLS Handshake Flow - Key Derivation Sequence

## Overview

This document describes the QUIC/TLS handshake sequence and when cryptographic keys are derived during the handshake process. Understanding this flow is critical for resolving the crypto key material assertion issue.

## QUIC/TLS Handshake Stages

### 1. Initial Stage (Pre-Handshake)

**Packets**: Initial Packets  
**Keys Used**: Pre-shared Initial Keys (stateless, no derivation needed)

**What Happens**:
- Client sends Initial packet with TLS ClientHello
- Initial keys are derived from connection ID (stateless)
- No TLS secrets needed at this stage
- Server responds with Initial packet containing TLS ServerHello

**Key Derivation**: None required - initial keys are stateless

### 2. Handshake Stage

**Packets**: Handshake Packets  
**Keys Used**: Derived Handshake Keys (from TLS handshake secrets)

**What Happens**:
- TLS handshake progresses via crypto data exchange
- TLS stack generates handshake secrets
- Handshake keys are derived from TLS secrets using HKDF
- Keys are installed into ngtcp2 connection via `update_key` callback
- Client and server exchange handshake packets with encrypted crypto data

**Key Derivation**:
- **When**: During TLS handshake, as secrets become available
- **Trigger**: TLS handshake progress (crypto data exchange)
- **Mechanism**: `update_key` callback receives new secrets
- **Installation**: Keys installed via `ngtcp2_crypto_derive_and_install_tx_key` / `ngtcp2_crypto_derive_and_install_rx_key`

### 3. 1-RTT Stage (Post-Handshake)

**Packets**: 1-RTT Packets (application data)  
**Keys Used**: Derived 1-RTT Keys (from TLS application secrets)

**What Happens**:
- TLS handshake completes
- TLS stack generates application secrets (1-RTT keys)
- 1-RTT keys are derived and installed
- Application data can now be sent (streams)

**Key Derivation**:
- **When**: After TLS handshake completes
- **Trigger**: TLS handshake completion
- **Mechanism**: `update_key` callback with application secrets
- **Installation**: Keys installed via crypto helpers

## Key Derivation Process

### When Keys Are Derived

Keys are **NOT** derived in the `client_initial` callback. Instead:

1. **Initial Keys**: Pre-shared, no derivation (available immediately)
2. **Handshake Keys**: Derived during TLS handshake via crypto data exchange
3. **1-RTT Keys**: Derived after handshake completes

### Key Derivation Flow

```
1. Connection Created
   ├─> client_initial callback
   │   └─> Sets TLS native handle (no keys derived yet)
   │
2. Initial Packet Sent
   ├─> Uses pre-shared initial keys (works fine)
   │
3. Server Responds
   ├─> recv_crypto_data callback
   │   └─> Processes incoming crypto data
   │   └─> Feeds data to TLS stack
   │   └─> TLS handshake progresses
   │
4. TLS Handshake Progress
   ├─> TLS stack generates secrets
   ├─> update_key callback triggered
   │   └─> Handshake keys derived
   │   └─> Keys installed via ngtcp2_crypto_derive_and_install_tx_key
   │
5. Handshake Packets Can Now Be Sent
   ├─> Keys are available
   └─> Handshake packets encrypted with handshake keys
```

### The Problem: Timing Issue

**Current Situation**:
- `client_initial` callback sets TLS native handle
- `send_initial_packet()` immediately calls `ngtcp2_conn_write_pkt()`
- ngtcp2 tries to write handshake packets (after initial packet)
- **Assertion fails**: Handshake keys not derived yet!

**Why This Happens**:
- ngtcp2 wants to send handshake packets after initial packet
- But handshake keys haven't been derived yet (need TLS handshake progress)
- Keys are derived only after server responds and TLS handshake progresses

## Callback Sequence

### client_initial Callback
**When**: Called during `ngtcp2_conn_client_new()`  
**Purpose**: Set up connection reference  
**What it does**:
- Sets TLS native handle
- Stores connection pointer
- **Does NOT derive keys**

### recv_crypto_data Callback
**When**: Called when crypto data is received  
**Purpose**: Process incoming crypto data and drive TLS handshake  
**What it does**:
- Receives crypto data at various encryption levels
- Feeds data to TLS stack (via `ngtcp2_crypto_recv_crypto_data_cb`)
- TLS handshake progresses
- **Keys are derived as handshake progresses**

### update_key Callback
**When**: Called when new TLS secrets are available  
**Purpose**: Derive and install keys from TLS secrets  
**What it does**:
- Receives new secrets from TLS stack
- Derives keys using HKDF
- Installs keys into ngtcp2 connection
- **This is where keys are actually derived**

## Current Implementation Issue

### What We're Doing

1. Create connection → `client_initial` callback → Set TLS handle
2. Immediately call `send_initial_packet()`
3. `send_initial_packet()` calls `ngtcp2_conn_write_pkt()`
4. ngtcp2 tries to write handshake packets
5. **Assertion**: Handshake keys not derived yet!

### What Should Happen

1. Create connection → `client_initial` callback → Set TLS handle
2. Call `send_initial_packet()` → Send initial packet (uses initial keys) ✅
3. **Wait for server response**
4. `recv_crypto_data` callback → Process incoming crypto data
5. TLS handshake progresses → Secrets generated
6. `update_key` callback → Derive and install handshake keys
7. **Now** handshake packets can be sent (keys are available) ✅

## Key Insights

### 1. Initial vs Handshake Packets

- **Initial Packets**: Use pre-shared initial keys (work immediately)
- **Handshake Packets**: Need derived handshake keys (not available until TLS handshake progresses)

### 2. Bidirectional Handshake

- QUIC handshake is bidirectional (client and server exchange packets)
- Keys are derived as the handshake progresses
- Cannot send handshake packets before keys are derived
- Must wait for server response to progress handshake

### 3. Crypto Data Drives Handshake

- Crypto data exchange drives TLS handshake
- TLS handshake progress generates secrets
- Secrets trigger key derivation
- Keys enable encrypted packet transmission

## What This Means for Our Implementation

### The Assertion Failure

The assertion `conn->in_pktns->crypto.tx.ckm` failed occurs because:
1. We're trying to send handshake packets
2. But handshake keys haven't been derived yet
3. Keys are only derived after server responds and TLS handshake progresses
4. We're calling `ngtcp2_conn_write_pkt()` too early (before keys are derived)

### Potential Solutions

1. **Wait for Server Response First**:
   - Send initial packet
   - Wait for server response
   - Process incoming packets
   - Let handshake progress
   - Then send handshake packets

2. **Only Send Initial Packets Initially**:
   - `send_initial_packet()` should only send initial packets
   - Don't try to send handshake packets yet
   - Wait for handshake to progress
   - Send handshake packets later (after keys derived)

3. **Drive TLS Handshake Forward**:
   - May need to manually drive TLS handshake before sending packets
   - Use `SSL_do_handshake` or similar
   - Ensure secrets are available before sending handshake packets

## Next Steps

1. **Investigate ngtcp2_conn_write_pkt behavior**:
   - Does it try to send handshake packets immediately?
   - Can we control what packet types it sends?
   - Should we only send initial packets first?

2. **Understand packet sending sequence**:
   - When should initial packets be sent?
   - When should handshake packets be sent?
   - What controls the packet type selection?

3. **Study ngtcp2 examples**:
   - How do examples handle packet sending?
   - What's the sequence of operations?
   - How do they avoid this issue?

## References

- QUIC RFC 9000: Handshake Overview
- ngtcp2 Programmer's Guide: TLS Integration
- ngtcp2 API Documentation: Callbacks
- CRYPTO_KEY_MATERIAL_ISSUE.md: Current issue documentation

