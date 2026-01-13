# QUIC Integration Research Notes

## Crypto Key Material Issue

### Problem
- Assertion: `conn_write_handshake_pkt: Assertion 'conn->in_pktns->crypto.tx.ckm' failed`
- Current state: TLS handle set manually, but crypto keys not derived
- Previous fix: Avoided using `ngtcp2_crypto_client_initial_cb` due to segfault

### ngtcp2_crypto_conn_ref Structure
```c
typedef struct ngtcp2_crypto_conn_ref {
  ngtcp2_crypto_get_conn get_conn;  // Callback to get connection pointer
  void *user_data;                   // User data for callback
} ngtcp2_crypto_conn_ref;

typedef ngtcp2_conn *(*ngtcp2_crypto_get_conn)(ngtcp2_crypto_conn_ref *conn_ref);
```

### Required Setup (from headers)
- Application must set `ngtcp2_crypto_conn_ref*` to SSL object via `SSL_set_app_data`
- `get_conn` field must be assigned to return `ngtcp2_conn*`
- This is required for `ngtcp2_crypto_ossl_configure_client_session`

### Timing Issue
- Connection doesn't exist when SSL is created
- Connection is created in `ngtcp2_conn_client_new`, which calls `client_initial` callback
- `client_initial` callback is called during connection creation

### Potential Solutions

#### Option 1: Store conn_ref as member, update in client_initial
- Store `ngtcp2_crypto_conn_ref` as member variable
- Set `SSL_set_app_data` before connection creation
- In `client_initial`, update conn_ref to point to actual connection
- Issue: Connection pointer passed to callback, but `conn_` member not set yet

#### Option 2: Use user_data in conn_ref
- Store `this` pointer in conn_ref.user_data
- `get_conn` callback accesses `this->conn_` via user_data
- Issue: Still timing problem - conn_ not set during client_initial call

#### Option 3: Use ngtcp2_crypto_client_initial_cb properly
- Set up SSL_set_app_data with conn_ref before calling crypto helper
- Helper should handle key derivation
- Previous attempt: segfaulted due to missing setup

## Research Findings

### ngtcp2 Example Code
- No examples found in system directories
- GitHub repository may have examples (not accessible in current environment)

### Crypto Helper Functions
- `ngtcp2_crypto_client_initial_cb`: Helper for client_initial callback
- `ngtcp2_crypto_recv_client_initial_cb`: Helper for server recv_client_initial callback
- Both require proper SSL_set_app_data setup

### Key Derivation
- `ngtcp2_crypto_derive_and_install_tx_key`: Derives and installs crypto keys
- Should be called automatically if setup is correct
- Manual key derivation may be needed if helpers aren't used

## Architecture Evaluation

### Current: Client-Server FTP Model
**Status**: Partially implemented
**Pros**:
- Simple, familiar model
- Clear separation of roles
- Easy authentication model

**Cons**:
- Server not fully implemented
- Missing proper QUIC handshake completion
- Incomplete file transfer implementation

### Alternative: P2P Model
**Examples**: quic-send, qcp, go-libp2p-quic-transport
**Pros**:
- No central server required
- Direct peer connections
- Better for ad-hoc transfers

**Cons**:
- More complex (NAT traversal, discovery)
- Security challenges
- May need signaling server

**Recommendation**: Complete current client-server model first, then evaluate P2P as future enhancement

