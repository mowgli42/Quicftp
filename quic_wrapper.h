// quic_wrapper.h
// QUIC library wrapper - provides abstraction layer for QUIC operations
// This will be implemented with ngtcp2 or another QUIC library

#ifndef QUIC_WRAPPER_H
#define QUIC_WRAPPER_H

#include "quic_common.h"
#include <string>
#include <functional>
#include <memory>

namespace quicftp {

// Forward declarations
struct QuicServerImpl;
struct QuicConnectionImpl;
struct QuicStreamImpl;

// QUIC Server wrapper
class QuicServerWrapper {
public:
  QuicServerWrapper();
  ~QuicServerWrapper();

  bool initialize(int port, const std::string& cert_path, const std::string& key_path);
  bool start_listening();
  void stop();
  bool is_listening() const;

  // Event loop
  void process_events(int timeout_ms = 100);
  
  // Get completed uploads (for test mode)
  struct PendingUpload {
    StreamId stream_id;
    std::string remote_path;
    std::vector<uint8_t> data;
  };
  std::vector<PendingUpload> get_pending_uploads();

  // Callback setters
  void set_connection_callback(ConnectionCallback on_connect, ConnectionCallback on_disconnect);
  void set_auth_callback(AuthCallback on_auth);
  void set_stream_callback(std::function<void(StreamId, const std::string&, StreamDataCallback)> on_stream);

private:
  std::unique_ptr<QuicServerImpl> impl_;
};

// QUIC Connection wrapper
class QuicConnectionWrapper {
public:
  QuicConnectionWrapper();
  ~QuicConnectionWrapper();

  std::string get_address() const;
  ConnectionState get_state() const;
  void close();

private:
  std::unique_ptr<QuicConnectionImpl> impl_;
  friend class QuicServerWrapper;
};

// QUIC Stream wrapper
class QuicStreamWrapper {
public:
  QuicStreamWrapper();
  ~QuicStreamWrapper();

  StreamId get_id() const;
  StreamState get_state() const;
  bool send_data(const uint8_t* data, size_t len);
  void close();

private:
  std::unique_ptr<QuicStreamImpl> impl_;
  friend class QuicServerWrapper;
};

} // namespace quicftp

#endif

