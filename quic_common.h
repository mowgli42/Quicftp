// quic_common.h
// Common QUIC types and forward declarations

#ifndef QUIC_COMMON_H
#define QUIC_COMMON_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace quicftp {

// Forward declarations for QUIC library types
// These will be replaced with actual ngtcp2 types when integrated
struct QuicConnection;
struct QuicStream;
struct QuicServer;

// Stream ID type
using StreamId = uint64_t;

// Stream state
enum class StreamState {
  Idle,
  Open,
  HalfClosed,
  Closed,
  Error
};

// Connection state
enum class ConnectionState {
  Disconnected,
  Connecting,
  Connected,
  Closing,
  Closed
};

// Transfer progress callback
using ProgressCallback = std::function<void(StreamId stream_id, size_t bytes_transferred, size_t total_bytes)>;

// Stream data callback (for receiving data)
using StreamDataCallback = std::function<bool(const uint8_t* data, size_t len)>;

// Connection event callbacks
using ConnectionCallback = std::function<void(const std::string& address)>;
using AuthCallback = std::function<void(const std::string& address, const std::string& cert_info, bool success)>;

} // namespace quicftp

#endif

