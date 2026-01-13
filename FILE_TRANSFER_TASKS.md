# Remaining Tasks for File Transfer Functionality

## Critical Blocking Issues

### 1. Fix Crypto Key Material Assertion (BLOCKING)
**Status**: ❌ Not fixed (research paused - see `CRYPTO_KEY_MATERIAL_ISSUE.md`)  
**Location**: `quicftp_client.cc:client_initial` callback  
**Error**: `conn_write_handshake_pkt: Assertion 'conn->in_pktns->crypto.tx.ckm' failed`  
**Impact**: Prevents QUIC handshake completion, blocks all file transfer operations

**What's been done**:
- ✅ Set up `SSL_set_app_data` with `ngtcp2_crypto_conn_ref` structure
- ✅ Fixed order of operations (configure SSL session before creating crypto context)
- ✅ Set TLS native handle in `client_initial` callback
- ❌ Crypto keys still not derived (assertion persists)

**What's needed**:
- Deeper understanding of QUIC/TLS handshake key derivation sequence
- Identify when and how handshake keys are derived
- May need to drive TLS handshake forward before sending packets
- See `CRYPTO_KEY_MATERIAL_ISSUE.md` for detailed analysis

**Priority**: **CRITICAL** - Must be fixed before any file transfer can work

---

## Connection and Handshake Tasks

### 2. Complete QUIC/TLS Handshake
**Status**: ⚠️ Partially implemented, blocked by issue #1  
**Location**: `quicftp_client.cc:initialize_ngtcp2_connection()` and `send_initial_packet()`  
**Current State**:
- Client sends initial packet ✅
- Client sets TLS handle ✅
- Crypto keys not derived ❌ (blocked by issue #1)
- Handshake packets not sent ❌ (blocked by issue #1)
- Server response not processed ⚠️

**What's needed**:
- Fix crypto key material issue (#1)
- Complete handshake packet exchange
- Process server handshake response
- Verify handshake completion before allowing stream operations

**Priority**: **CRITICAL** - Required for connection establishment

### 3. Implement Packet Processing Loop
**Status**: ⚠️ Partially implemented  
**Location**: `quicftp_client.cc:process_incoming_packets()`  
**Current State**:
- Function exists but may not be called properly
- Needs to be integrated into connection/stream lifecycle
- Must handle handshake packets, stream data, ACKs

**What's needed**:
- Ensure `process_incoming_packets()` is called during connection
- Process all incoming packet types (handshake, stream data, ACKs)
- Integrate with event loop or connection state management
- Handle server responses during handshake

**Priority**: **HIGH** - Required for bidirectional communication

### 4. Implement Send Pending Packets
**Status**: ⚠️ Partially implemented  
**Location**: `quicftp_client.cc:send_pending_packets()`  
**Current State**:
- Function exists
- Needs to be called after writing data
- Must ensure all pending packets are sent

**What's needed**:
- Call `send_pending_packets()` after `ngtcp2_conn_writev_stream`
- Ensure all pending packets are flushed
- Handle retransmissions
- Integrate with packet processing loop

**Priority**: **HIGH** - Required for reliable data transfer

---

## Stream Operations Tasks

### 5. Implement Receive Data (Client)
**Status**: ❌ Not implemented (stubbed)  
**Location**: `quicftp_client.cc:receive_data()`  
**Current State**: 
```cpp
bool QuicClientWrapper::receive_data(StreamId stream_id, std::function<bool(const uint8_t*, size_t)> callback) {
  if (!connected_) return false;
  // TODO: Receive data from QUIC stream
  return true;
}
```

**What's needed**:
- Implement `recv_stream_data` callback in ngtcp2 callbacks
- Store received data per stream
- Call user callback with received data
- Handle stream completion
- Support download file transfers

**Priority**: **HIGH** - Required for download functionality

### 6. Implement Close Stream
**Status**: ❌ Not implemented (stubbed)  
**Location**: `quicftp_client.cc:close_stream()`  
**Current State**:
```cpp
void QuicClientWrapper::close_stream(StreamId stream_id) {
  // TODO: Close QUIC stream
}
```

**What's needed**:
- Call `ngtcp2_conn_shutdown_stream` or equivalent
- Clean up stream state
- Send stream close packets
- Handle stream reset if needed

**Priority**: **MEDIUM** - Important for proper cleanup

### 7. Implement Stream Data Callback
**Status**: ⚠️ Not implemented  
**Location**: `quicftp_client.cc:initialize_ngtcp2_connection()` callbacks  
**Current State**: `callbacks.recv_stream_data = nullptr;`

**What's needed**:
- Implement `recv_stream_data` callback
- Store received data per stream ID
- Handle stream data completion
- Trigger user callbacks
- Support both upload (server-side) and download (client-side)

**Priority**: **HIGH** - Required for receiving data

### 8. Implement Stream Open/Close Callbacks
**Status**: ⚠️ Not implemented  
**Location**: `quicftp_client.cc:initialize_ngtcp2_connection()` callbacks  
**Current State**: `callbacks.stream_open = nullptr;` and `callbacks.stream_close = nullptr;`

**What's needed**:
- Track stream state (open/closed)
- Handle stream lifecycle events
- Clean up stream resources when closed
- Update connection state based on streams

**Priority**: **MEDIUM** - Important for stream management

---

## Server-Side Tasks

### 9. Complete Server Stream Handling
**Status**: ⚠️ Partially implemented  
**Location**: `quic_wrapper.cc:server_recv_stream_data()`  
**Current State**:
- Server receives stream data ✅
- Server accumulates data ✅
- Server processes upload commands ✅
- May need improvements for reliability

**What's needed**:
- Verify server stream handling works correctly
- Ensure server sends responses
- Handle stream completion properly
- Test end-to-end file upload

**Priority**: **HIGH** - Required for server functionality

### 10. Implement Server Response Sending
**Status**: ⚠️ Unknown  
**Location**: Server implementation  
**Current State**: Need to verify if server sends responses

**What's needed**:
- Server should send responses to client commands
- Server should send file data for downloads
- Server should send ACKs and other control packets
- Ensure server can write to streams

**Priority**: **HIGH** - Required for download functionality

---

## File Transfer Protocol Tasks

### 11. Verify Upload Protocol
**Status**: ⚠️ Partially implemented  
**Location**: `quicftp_client.cc:upload_file()`  
**Current State**:
- Client sends "UPLOAD <path>\n" command ✅
- Client sends file data ✅
- Server receives and saves files ✅
- Need to verify end-to-end works

**What's needed**:
- Test complete upload flow once handshake works
- Verify server responses
- Handle errors properly
- Verify file integrity

**Priority**: **MEDIUM** - Should work once handshake is fixed

### 12. Complete Download Protocol
**Status**: ⚠️ Partially implemented  
**Location**: `quicftp_client.cc:download_file()`  
**Current State**:
- Client sends "DOWNLOAD <path>\n" command ✅
- Client calls `receive_data()` ❌ (not implemented)
- Server reads file and sends data ⚠️ (needs verification)

**What's needed**:
- Implement `receive_data()` (#5)
- Verify server sends file data
- Test complete download flow
- Handle errors and edge cases

**Priority**: **HIGH** - Required for download functionality

---

## Testing and Verification Tasks

### 13. End-to-End File Transfer Test
**Status**: ❌ Cannot test (blocked by issue #1)  
**What's needed**:
- Fix blocking issues first (#1)
- Test single file upload
- Test single file download
- Verify file integrity
- Test error handling

**Priority**: **HIGH** - Required to verify functionality

### 14. Multiple File Transfer Test
**Status**: ❌ Cannot test  
**What's needed**:
- Test parallel file uploads
- Test parallel file downloads
- Test mixed upload/download
- Verify stream multiplexing works

**Priority**: **MEDIUM** - Important feature but not blocking

---

## Summary by Priority

### CRITICAL (Must fix before file transfer works)
1. ✅ **Fix Crypto Key Material Assertion** - Blocks handshake
2. ✅ **Complete QUIC/TLS Handshake** - Required for connection
3. ✅ **Implement Packet Processing Loop** - Required for communication

### HIGH (Required for basic functionality)
4. ✅ **Implement Send Pending Packets** - Required for data transfer
5. ✅ **Implement Receive Data (Client)** - Required for downloads
6. ✅ **Implement Stream Data Callback** - Required for receiving
7. ✅ **Complete Server Stream Handling** - Required for server
8. ✅ **Implement Server Response Sending** - Required for downloads
9. ✅ **Complete Download Protocol** - Required feature

### MEDIUM (Important but not blocking)
10. ✅ **Implement Close Stream** - Cleanup
11. ✅ **Implement Stream Open/Close Callbacks** - Stream management
12. ✅ **Verify Upload Protocol** - Testing
13. ✅ **End-to-End File Transfer Test** - Verification
14. ✅ **Multiple File Transfer Test** - Advanced feature

---

## Recommended Work Order

1. **First**: Fix crypto key material issue (#1) - This is blocking everything
2. **Second**: Complete handshake (#2) and packet processing (#3)
3. **Third**: Implement receive data (#5) and stream callbacks (#7)
4. **Fourth**: Test end-to-end upload (#13)
5. **Fifth**: Complete download protocol (#12) and test downloads
6. **Sixth**: Cleanup and polish (close streams, callbacks, etc.)

---

## Current State Assessment

**What Works**:
- ✅ Client connection setup (UDP socket, SSL context)
- ✅ Client sends initial packet
- ✅ Stream creation code exists
- ✅ File upload sending code exists
- ✅ Server receives and saves files (via TestBridge)
- ✅ File download sending code exists
- ✅ High-level API structure (`upload_file`, `download_file`)

**What's Blocked**:
- ❌ QUIC handshake completion (crypto key material issue)
- ❌ Stream creation (requires handshake)
- ❌ Data sending (requires handshake)
- ❌ Data receiving (not implemented + requires handshake)
- ❌ End-to-end file transfer (blocked by above)

**Estimated Effort**:
- Crypto key material fix: 4-8 hours (research + implementation)
- Complete handshake: 2-4 hours
- Implement receive data: 4-6 hours
- Testing and debugging: 4-8 hours
- **Total**: ~14-26 hours of focused development

