// test_bridge.h
// Simple test bridge for QUIC stubs - allows client/server communication for testing
// This is a temporary solution until real QUIC library is integrated
// Uses file-based queue for inter-process communication

#ifndef TEST_BRIDGE_H
#define TEST_BRIDGE_H

#include "quic_common.h"
#include <string>
#include <vector>
#include <mutex>
#include <fstream>

namespace quicftp {

// File-based message queue for inter-process communication
class TestBridge {
public:
  static TestBridge& instance() {
    static TestBridge inst;
    return inst;
  }

  // Client side: send data
  bool send_to_server(const std::string& server_addr, StreamId stream_id, const uint8_t* data, size_t len);
  
  // Server side: receive data
  bool receive_from_client(std::string& client_addr, StreamId& stream_id, std::vector<uint8_t>& data);
  
  // Check if data is available
  bool has_data() const;

private:
  TestBridge();
  ~TestBridge() = default;
  TestBridge(const TestBridge&) = delete;
  TestBridge& operator=(const TestBridge&) = delete;

  std::string queue_file_path_;
  mutable std::mutex mutex_;
  
  std::string get_queue_path() const;
};

} // namespace quicftp

#endif

