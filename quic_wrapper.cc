// quic_wrapper.cc
// Stub implementation - will be replaced with actual QUIC library integration

#include "quic_wrapper.h"
#include "test_bridge.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

namespace quicftp {

// Stub implementations - these will be replaced with actual QUIC library calls
struct QuicServerImpl {
  int port_;
  bool listening_;
  std::string cert_path_;
  std::string key_path_;
  
  ConnectionCallback on_connect_;
  ConnectionCallback on_disconnect_;
  AuthCallback on_auth_;
  std::function<void(StreamId, const std::string&, StreamDataCallback)> on_stream_;
  
  // Test mode: track received data
  std::map<StreamId, std::vector<uint8_t>> stream_data_;
  std::map<StreamId, std::string> stream_commands_;
};

struct QuicConnectionImpl {
  std::string address_;
  ConnectionState state_;
};

struct QuicStreamImpl {
  StreamId id_;
  StreamState state_;
};

QuicServerWrapper::QuicServerWrapper() : impl_(std::make_unique<QuicServerImpl>()) {
  impl_->listening_ = false;
  impl_->port_ = 0;
}

QuicServerWrapper::~QuicServerWrapper() {
  stop();
}

bool QuicServerWrapper::initialize(int port, const std::string& cert_path, const std::string& key_path) {
  impl_->port_ = port;
  impl_->cert_path_ = cert_path;
  impl_->key_path_ = key_path;
  // TODO: Initialize actual QUIC server library here
  return true;
}

bool QuicServerWrapper::start_listening() {
  // TODO: Start actual QUIC server listening on port
  impl_->listening_ = true;
  return true;
}

void QuicServerWrapper::stop() {
  impl_->listening_ = false;
  // TODO: Stop QUIC server and close connections
}

bool QuicServerWrapper::is_listening() const {
  return impl_->listening_;
}

void QuicServerWrapper::process_events(int timeout_ms) {
  if (!impl_->listening_) return;
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      bool has_data = TestBridge::instance().has_data();
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"L\",\"location\":\"quic_wrapper.cc:74\",\"message\":\"QuicServerWrapper::process_events() called\",\"data\":{\"timeout_ms\":" << timeout_ms << ",\"listening\":" << (impl_->listening_ ? "true" : "false") << ",\"bridge_has_data\":" << (has_data ? "true" : "false") << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  // Test mode: Process messages from test bridge
  std::string client_addr;
  StreamId stream_id;
  std::vector<uint8_t> data;
  
  int messages_processed = 0;
  while (TestBridge::instance().receive_from_client(client_addr, stream_id, data)) {
    messages_processed++;
    // Parse command (first line should be "UPLOAD path" or "DOWNLOAD path")
    std::string message(reinterpret_cast<const char*>(data.data()), data.size());
    
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"I\",\"location\":\"quic_wrapper.cc:69\",\"message\":\"Received data from client\",\"data\":{\"stream_id\":" << stream_id << ",\"data_size\":" << data.size() << ",\"message_preview\":\"" << message.substr(0, 50) << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    
    // Check if this is a command (starts with UPLOAD or DOWNLOAD)
    if (message.find("UPLOAD ") == 0) {
      // Extract path
      size_t path_start = 7; // "UPLOAD "
      size_t path_end = message.find('\n', path_start);
      if (path_end == std::string::npos) {
        // #region agent log
        {
          std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
          if (log_file.is_open()) {
            log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"M\",\"location\":\"quic_wrapper.cc:100\",\"message\":\"UPLOAD command missing newline\",\"data\":{\"stream_id\":" << stream_id << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            log_file.close();
          }
        }
        // #endregion
        continue;
      }
      std::string remote_path = message.substr(path_start, path_end - path_start);
      
      // Store command for this stream
      impl_->stream_commands_[stream_id] = remote_path;
      impl_->stream_data_[stream_id].clear();
      
      // #region agent log
      {
        std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
        if (log_file.is_open()) {
          log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"I\",\"location\":\"quic_wrapper.cc:115\",\"message\":\"Parsed UPLOAD command\",\"data\":{\"stream_id\":" << stream_id << ",\"remote_path\":\"" << remote_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
          log_file.close();
        }
      }
      // #endregion
      
      // Extract file data (everything after the newline) - if any in this message
      if (path_end + 1 < message.length()) {
        std::string file_data = message.substr(path_end + 1);
        impl_->stream_data_[stream_id].insert(impl_->stream_data_[stream_id].end(), file_data.begin(), file_data.end());
      }
    } else if (message.find("DOWNLOAD ") == 0) {
      // Handle download request
      size_t path_start = 9; // "DOWNLOAD "
      size_t path_end = message.find('\n', path_start);
      if (path_end == std::string::npos) {
        continue;
      }
      std::string remote_path = message.substr(path_start, path_end - path_start);
      impl_->stream_commands_[stream_id] = remote_path;
      
      // #region agent log
      {
        std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
        if (log_file.is_open()) {
          log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"I\",\"location\":\"quic_wrapper.cc:145\",\"message\":\"Parsed DOWNLOAD command\",\"data\":{\"stream_id\":" << stream_id << ",\"remote_path\":\"" << remote_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
          log_file.close();
        }
      }
      // #endregion
    } else {
      // This is file data (continuation of upload) - accumulate it
      if (impl_->stream_commands_.find(stream_id) != impl_->stream_commands_.end()) {
        // #region agent log
        {
          std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
          if (log_file.is_open()) {
            log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"M\",\"location\":\"quic_wrapper.cc:158\",\"message\":\"Accumulating file data chunk\",\"data\":{\"stream_id\":" << stream_id << ",\"chunk_size\":" << data.size() << ",\"total_size\":" << impl_->stream_data_[stream_id].size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            log_file.close();
          }
        }
        // #endregion
        impl_->stream_data_[stream_id].insert(impl_->stream_data_[stream_id].end(), data.begin(), data.end());
      } else {
        // #region agent log
        {
          std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
          if (log_file.is_open()) {
            log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"M\",\"location\":\"quic_wrapper.cc:168\",\"message\":\"Received data for unknown stream\",\"data\":{\"stream_id\":" << stream_id << ",\"data_size\":" << data.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            log_file.close();
          }
        }
        // #endregion
      }
    }
  }
  
  // #region agent log
  if (messages_processed > 0) {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"L\",\"location\":\"quic_wrapper.cc:195\",\"message\":\"Processed messages from bridge\",\"data\":{\"messages_processed\":" << messages_processed << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  // Completed uploads will be retrieved via get_pending_uploads()
  
  // TODO: Process QUIC events from library
  // For now, just sleep to prevent busy waiting
  std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}

void QuicServerWrapper::set_connection_callback(ConnectionCallback on_connect, ConnectionCallback on_disconnect) {
  impl_->on_connect_ = on_connect;
  impl_->on_disconnect_ = on_disconnect;
}

void QuicServerWrapper::set_auth_callback(AuthCallback on_auth) {
  impl_->on_auth_ = on_auth;
}

void QuicServerWrapper::set_stream_callback(std::function<void(StreamId, const std::string&, StreamDataCallback)> on_stream) {
  impl_->on_stream_ = on_stream;
}

std::vector<QuicServerWrapper::PendingUpload> QuicServerWrapper::get_pending_uploads() {
  std::vector<PendingUpload> uploads;
  
  for (auto it = impl_->stream_commands_.begin(); it != impl_->stream_commands_.end();) {
    StreamId sid = it->first;
    std::string remote_path = it->second;
    
    // Check if we have data for this stream (upload complete)
    if (impl_->stream_data_.find(sid) != impl_->stream_data_.end() && 
        !impl_->stream_data_[sid].empty()) {
      PendingUpload upload;
      upload.stream_id = sid;
      upload.remote_path = remote_path;
      upload.data = std::move(impl_->stream_data_[sid]);
      uploads.push_back(upload);
      
      // Remove from tracking
      impl_->stream_commands_.erase(it++);
      impl_->stream_data_.erase(sid);
    } else {
      it++;
    }
  }
  
  return uploads;
}

QuicConnectionWrapper::QuicConnectionWrapper() : impl_(std::make_unique<QuicConnectionImpl>()) {
  impl_->state_ = ConnectionState::Disconnected;
}

QuicConnectionWrapper::~QuicConnectionWrapper() = default;

std::string QuicConnectionWrapper::get_address() const {
  return impl_->address_;
}

ConnectionState QuicConnectionWrapper::get_state() const {
  return impl_->state_;
}

void QuicConnectionWrapper::close() {
  impl_->state_ = ConnectionState::Closing;
  // TODO: Close QUIC connection
}

QuicStreamWrapper::QuicStreamWrapper() : impl_(std::make_unique<QuicStreamImpl>()) {
  impl_->state_ = StreamState::Idle;
  impl_->id_ = 0;
}

QuicStreamWrapper::~QuicStreamWrapper() = default;

StreamId QuicStreamWrapper::get_id() const {
  return impl_->id_;
}

StreamState QuicStreamWrapper::get_state() const {
  return impl_->state_;
}

bool QuicStreamWrapper::send_data(const uint8_t* data, size_t len) {
  // TODO: Send data over QUIC stream
  return true;
}

void QuicStreamWrapper::close() {
  impl_->state_ = StreamState::Closed;
  // TODO: Close QUIC stream
}

} // namespace quicftp

