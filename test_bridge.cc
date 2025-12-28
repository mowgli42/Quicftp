// test_bridge.cc

#include "test_bridge.h"
#include "quic_common.h"
#include <fstream>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <iterator>

namespace quicftp {

TestBridge::TestBridge() {
  queue_file_path_ = get_queue_path();
}

std::string TestBridge::get_queue_path() const {
  const char* tmpdir = std::getenv("TMPDIR");
  if (!tmpdir) tmpdir = std::getenv("TMP");
  if (!tmpdir) tmpdir = "/tmp";
  return std::string(tmpdir) + "/quicftp_test_bridge.queue";
}

bool TestBridge::send_to_server(const std::string& server_addr, StreamId stream_id, const uint8_t* data, size_t len) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Write message to file queue (append mode)
  std::ofstream queue_file(queue_file_path_, std::ios::binary | std::ios::app);
  if (!queue_file.is_open()) {
    return false;
  }
  
  // Format: server_addr\nstream_id\nlen\ndata
  queue_file << server_addr << "\n";
  queue_file << stream_id << "\n";
  queue_file << len << "\n";
  queue_file.write(reinterpret_cast<const char*>(data), len);
  queue_file << "\n"; // separator
  queue_file.flush();
  queue_file.close();
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      std::string preview(data, data + std::min(len, size_t(50)));
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:35\",\"message\":\"TestBridge: message written to file\",\"data\":{\"server_addr\":\"" << server_addr << "\",\"stream_id\":" << stream_id << ",\"len\":" << len << ",\"queue_file\":\"" << queue_file_path_ << "\",\"preview\":\"" << preview << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  return true;
}

bool TestBridge::receive_from_client(std::string& client_addr, StreamId& stream_id, std::vector<uint8_t>& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:60\",\"message\":\"TestBridge: attempting to read from file\",\"data\":{\"queue_file\":\"" << queue_file_path_ << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  std::ifstream queue_file(queue_file_path_, std::ios::binary);
  if (!queue_file.is_open()) {
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:68\",\"message\":\"TestBridge: queue file not open\",\"data\":{\"queue_file\":\"" << queue_file_path_ << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    return false;
  }
  
  if (queue_file.peek() == std::ifstream::traits_type::eof()) {
    queue_file.close();
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:80\",\"message\":\"TestBridge: queue file is empty\",\"data\":{\"queue_file\":\"" << queue_file_path_ << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    return false;
  }
  
  // Read message: server_addr\nstream_id\nlen\ndata\n
  std::getline(queue_file, client_addr);
  if (queue_file.fail()) {
    queue_file.close();
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:95\",\"message\":\"TestBridge: failed to read server_addr\",\"data\":{},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    return false;
  }
  
  std::string stream_id_str;
  std::getline(queue_file, stream_id_str);
  if (queue_file.fail()) {
    queue_file.close();
    return false;
  }
  stream_id = std::stoull(stream_id_str);
  
  std::string len_str;
  std::getline(queue_file, len_str);
  if (queue_file.fail()) {
    queue_file.close();
    return false;
  }
  size_t len = std::stoull(len_str);
  
  data.resize(len);
  queue_file.read(reinterpret_cast<char*>(data.data()), len);
  size_t bytes_read = queue_file.gcount();
  if (bytes_read != len) {
    queue_file.close();
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:125\",\"message\":\"TestBridge: incomplete data read\",\"data\":{\"expected\":" << len << ",\"read\":" << bytes_read << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    return false;
  }
  
  // Read separator newline
  char sep;
  queue_file.get(sep);
  if (sep != '\n') {
    queue_file.close();
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:140\",\"message\":\"TestBridge: missing separator\",\"data\":{\"sep_char\":" << int(sep) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    return false;
  }
  
  std::streampos current_pos = queue_file.tellg();
  queue_file.close();
  
  // Remove processed message from file (read remaining, write back)
  std::ifstream remaining_file(queue_file_path_, std::ios::binary);
  remaining_file.seekg(current_pos);
  std::vector<char> remaining((std::istreambuf_iterator<char>(remaining_file)),
                              std::istreambuf_iterator<char>());
  remaining_file.close();
  
  // Rewrite file with remaining messages
  std::ofstream write_file(queue_file_path_, std::ios::binary | std::ios::trunc);
  if (write_file.is_open() && !remaining.empty()) {
    write_file.write(remaining.data(), remaining.size());
  }
  write_file.close();
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"N\",\"location\":\"test_bridge.cc:165\",\"message\":\"TestBridge: message read successfully\",\"data\":{\"client_addr\":\"" << client_addr << "\",\"stream_id\":" << stream_id << ",\"len\":" << len << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  return true;
}

bool TestBridge::has_data() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ifstream queue_file(queue_file_path_, std::ios::binary);
  bool has_data = queue_file.is_open() && queue_file.peek() != std::ifstream::traits_type::eof();
  queue_file.close();
  return has_data;
}

} // namespace quicftp

