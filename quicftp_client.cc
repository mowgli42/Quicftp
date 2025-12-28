// quicftp_client.cc

#include "quicftp_client.h"
#include "quic_common.h"
#include "quic_wrapper.h"
#include "stream_manager.h"
#include "test_bridge.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <map>
#include <vector>
#include <functional>
#include <chrono>

namespace quicftp {

// Forward declaration for QUIC client wrapper
class QuicClientWrapper {
public:
  QuicClientWrapper();
  ~QuicClientWrapper();

  bool connect(const std::string& server_address);
  bool authenticate(const std::string& cert_path);
  void disconnect();
  bool is_connected() const;

  // Stream operations
  bool create_stream(StreamId& stream_id);
  bool send_data(StreamId stream_id, const uint8_t* data, size_t len);
  bool receive_data(StreamId stream_id, std::function<bool(const uint8_t*, size_t)> callback);
  void close_stream(StreamId stream_id);

private:
  bool connected_;
  std::string server_address_;
  std::string cert_path_;
  // TODO: Add actual QUIC client connection
};

// Stub implementation
QuicClientWrapper::QuicClientWrapper() : connected_(false) {}
QuicClientWrapper::~QuicClientWrapper() { disconnect(); }

bool QuicClientWrapper::connect(const std::string& server_address) {
  server_address_ = server_address;
  // TODO: Establish QUIC connection
  connected_ = true;
  return true;
}

bool QuicClientWrapper::authenticate(const std::string& cert_path) {
  cert_path_ = cert_path;
  // TODO: Perform certificate-based authentication
  return true;
}

void QuicClientWrapper::disconnect() {
  if (connected_) {
    // TODO: Close QUIC connection
    connected_ = false;
  }
}

bool QuicClientWrapper::is_connected() const {
  return connected_;
}

bool QuicClientWrapper::create_stream(StreamId& stream_id) {
  if (!connected_) return false;
  // TODO: Create QUIC stream
  static StreamId next_id = 1;
  stream_id = next_id++;
  return true;
}

bool QuicClientWrapper::send_data(StreamId stream_id, const uint8_t* data, size_t len) {
  if (!connected_) return false;
  // Test mode: Send data via test bridge
  return TestBridge::instance().send_to_server(server_address_, stream_id, data, len);
}

bool QuicClientWrapper::receive_data(StreamId stream_id, std::function<bool(const uint8_t*, size_t)> callback) {
  if (!connected_) return false;
  // TODO: Receive data from QUIC stream
  return true;
}

void QuicClientWrapper::close_stream(StreamId stream_id) {
  // TODO: Close QUIC stream
}

// Client implementation
class Client::Impl {
public:
  std::unique_ptr<QuicClientWrapper> quic_client_;
  std::unique_ptr<StreamManager> stream_manager_;
  bool authenticated_;
  std::mutex mutex_;
  std::function<void(StreamId, size_t, size_t)> progress_callback_;

  Impl() : authenticated_(false) {
    quic_client_ = std::make_unique<QuicClientWrapper>();
    stream_manager_ = std::make_unique<StreamManager>();
  }
};

Client::Client() : impl_(std::make_unique<Impl>()) {
}

Client::~Client() {
  disconnect();
}

bool Client::connect(const std::string& server) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  
  if (impl_->quic_client_->is_connected()) {
    std::cerr << "Already connected to " << impl_->quic_client_->is_connected() << std::endl;
    return false;
  }

  if (!impl_->quic_client_->connect(server)) {
    std::cerr << "Failed to connect to " << server << std::endl;
    return false;
  }

  return true;
}

bool Client::authenticate(const std::string& cert_path) {
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"quicftp_client.cc:131\",\"message\":\"authenticate() called\",\"data\":{\"cert_path\":\"" << cert_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  std::lock_guard<std::mutex> lock(impl_->mutex_);

  if (!impl_->quic_client_->is_connected()) {
    std::cerr << "Not connected" << std::endl;
    return false;
  }

  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      bool exists = std::filesystem::exists(cert_path);
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"D\",\"location\":\"quicftp_client.cc:143\",\"message\":\"filesystem::exists() check\",\"data\":{\"cert_path\":\"" << cert_path << "\",\"exists\":" << (exists ? "true" : "false") << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion

  if (!std::filesystem::exists(cert_path)) {
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"quicftp_client.cc:150\",\"message\":\"Certificate file not found error\",\"data\":{\"cert_path\":\"" << cert_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    std::cerr << "Certificate file not found: " << cert_path << std::endl;
    return false;
  }

  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"quicftp_client.cc:155\",\"message\":\"About to call quic_client_->authenticate()\",\"data\":{\"cert_path\":\"" << cert_path << "\",\"is_connected\":" << (impl_->quic_client_->is_connected() ? "true" : "false") << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion

  if (!impl_->quic_client_->authenticate(cert_path)) {
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"quicftp_client.cc:162\",\"message\":\"quic_client_->authenticate() returned false\",\"data\":{\"cert_path\":\"" << cert_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    std::cerr << "Authentication failed" << std::endl;
    return false;
  }

  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\",\"location\":\"quicftp_client.cc:175\",\"message\":\"Authentication successful\",\"data\":{\"cert_path\":\"" << cert_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion

  impl_->authenticated_ = true;
  return true;
}

bool Client::login(const std::string& username, const std::string& password) {
  // For certificate-based auth, login may not be needed
  // But we'll keep the API for compatibility
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  
  if (!impl_->authenticated_) {
    std::cerr << "Not authenticated" << std::endl;
    return false;
  }

  // TODO: Implement login if needed by protocol
  return true;
}

bool Client::upload_file(const std::string& local_path, const std::string& remote_path) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);

  if (!impl_->authenticated_) {
    std::cerr << "Not authenticated" << std::endl;
    return false;
  }

  if (!std::filesystem::exists(local_path)) {
    std::cerr << "Local file not found: " << local_path << std::endl;
    return false;
  }

  // Create stream for file transfer
  StreamId stream_id;
  if (!impl_->quic_client_->create_stream(stream_id)) {
    std::cerr << "Failed to create stream for upload" << std::endl;
    return false;
  }

  // Send remote path first
  std::string path_msg = "UPLOAD " + remote_path + "\n";
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"K\",\"location\":\"quicftp_client.cc:247\",\"message\":\"Sending UPLOAD command\",\"data\":{\"stream_id\":" << stream_id << ",\"remote_path\":\"" << remote_path << "\",\"path_msg_length\":" << path_msg.length() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  if (!impl_->quic_client_->send_data(stream_id, 
                                      reinterpret_cast<const uint8_t*>(path_msg.c_str()),
                                      path_msg.length())) {
    std::cerr << "Failed to send upload command" << std::endl;
    return false;
  }
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"K\",\"location\":\"quicftp_client.cc:256\",\"message\":\"UPLOAD command sent successfully\",\"data\":{\"stream_id\":" << stream_id << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion

  // Read and send file
  std::ifstream file(local_path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << local_path << std::endl;
    return false;
  }

  // Get file size for progress tracking
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  const size_t chunk_size = 64 * 1024; // 64KB chunks
  std::vector<uint8_t> buffer(chunk_size);
  size_t total_sent = 0;

  while (file.read(reinterpret_cast<char*>(buffer.data()), chunk_size) || file.gcount() > 0) {
    size_t bytes_read = file.gcount();
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"K\",\"location\":\"quicftp_client.cc:275\",\"message\":\"Sending file chunk\",\"data\":{\"stream_id\":" << stream_id << ",\"bytes_read\":" << bytes_read << ",\"total_sent\":" << total_sent << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    if (!impl_->quic_client_->send_data(stream_id, buffer.data(), bytes_read)) {
      std::cerr << "Failed to send file data at " << total_sent << " bytes" << std::endl;
      file.close();
      return false;
    }
    total_sent += bytes_read;
    
    // Progress tracking (could be enhanced with callbacks)
    if (file_size > 0 && total_sent % (1024 * 1024) == 0) { // Log every MB
      double percent = (static_cast<double>(total_sent) / file_size) * 100.0;
      std::cout << "Upload progress: " << total_sent << "/" << file_size 
                << " bytes (" << percent << "%)" << std::endl;
    }
  }

  if (!file.eof() && file.fail()) {
    std::cerr << "Error reading file: " << local_path << std::endl;
    file.close();
    return false;
  }

  file.close();
  impl_->quic_client_->close_stream(stream_id);
  std::cout << "Upload completed: " << total_sent << " bytes" << std::endl;
  return true;
}

bool Client::download_file(const std::string& remote_path, const std::string& local_path) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);

  if (!impl_->authenticated_) {
    std::cerr << "Not authenticated" << std::endl;
    return false;
  }

  // Create stream for file transfer
  StreamId stream_id;
  if (!impl_->quic_client_->create_stream(stream_id)) {
    std::cerr << "Failed to create stream for download" << std::endl;
    return false;
  }

  // Send download request
  std::string path_msg = "DOWNLOAD " + remote_path + "\n";
  if (!impl_->quic_client_->send_data(stream_id,
                                      reinterpret_cast<const uint8_t*>(path_msg.c_str()),
                                      path_msg.length())) {
    std::cerr << "Failed to send download command" << std::endl;
    return false;
  }

  // Create local file
  std::ofstream file(local_path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to create file: " << local_path << std::endl;
    return false;
  }

  // Receive file data with progress tracking
  size_t total_received = 0;
  bool success = impl_->quic_client_->receive_data(stream_id,
    [&file, &total_received](const uint8_t* data, size_t len) -> bool {
      file.write(reinterpret_cast<const char*>(data), len);
      if (!file.good()) {
        return false;
      }
      total_received += len;
      
      // Progress tracking (could be enhanced with callbacks)
      if (total_received % (1024 * 1024) == 0) { // Log every MB
        std::cout << "Download progress: " << total_received << " bytes" << std::endl;
      }
      return true;
    }
  );

  file.close();
  impl_->quic_client_->close_stream(stream_id);
  
  if (success) {
    std::cout << "Download completed: " << total_received << " bytes" << std::endl;
  } else {
    std::cerr << "Download failed after receiving " << total_received << " bytes" << std::endl;
  }
  
  return success;
}

bool Client::logout() {
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  impl_->authenticated_ = false;
  // TODO: Send logout command if needed
  return true;
}

bool Client::upload_files(const std::vector<std::pair<std::string, std::string>>& files) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);

  if (!impl_->authenticated_) {
    std::cerr << "Not authenticated" << std::endl;
    return false;
  }

  // Create streams for all files
  std::vector<StreamId> stream_ids;
  for (const auto& [local_path, remote_path] : files) {
    if (!std::filesystem::exists(local_path)) {
      std::cerr << "Local file not found: " << local_path << std::endl;
      continue;
    }
    
    size_t file_size = std::filesystem::file_size(local_path);
    StreamId stream_id = impl_->stream_manager_->create_stream(remote_path, file_size, 0, true);
    stream_ids.push_back(stream_id);
  }

  // Upload files in parallel (simplified - would use actual parallel streams)
  bool all_success = true;
  for (size_t i = 0; i < files.size() && i < stream_ids.size(); ++i) {
    const auto& [local_path, remote_path] = files[i];
    StreamId stream_id = stream_ids[i];
    
    if (!upload_file(local_path, remote_path)) {
      all_success = false;
      impl_->stream_manager_->error_stream(stream_id, "Upload failed");
    } else {
      impl_->stream_manager_->complete_stream(stream_id);
    }
  }

  return all_success;
}

bool Client::download_files(const std::vector<std::pair<std::string, std::string>>& files) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);

  if (!impl_->authenticated_) {
    std::cerr << "Not authenticated" << std::endl;
    return false;
  }

  // Create streams for all files
  std::vector<StreamId> stream_ids;
  for (const auto& [remote_path, local_path] : files) {
    StreamId stream_id = impl_->stream_manager_->create_stream(remote_path, 0, 0, false);
    stream_ids.push_back(stream_id);
  }

  // Download files in parallel (simplified - would use actual parallel streams)
  bool all_success = true;
  for (size_t i = 0; i < files.size() && i < stream_ids.size(); ++i) {
    const auto& [remote_path, local_path] = files[i];
    StreamId stream_id = stream_ids[i];
    
    if (!download_file(remote_path, local_path)) {
      all_success = false;
      impl_->stream_manager_->error_stream(stream_id, "Download failed");
    } else {
      impl_->stream_manager_->complete_stream(stream_id);
    }
  }

  return all_success;
}

void Client::set_progress_callback(std::function<void(StreamId, size_t, size_t)> callback) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  impl_->progress_callback_ = callback;
}

bool Client::cancel_transfer(StreamId stream_id) {
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  
  auto* stream_info = impl_->stream_manager_->get_stream(stream_id);
  if (!stream_info || stream_info->state != StreamState::Open) {
    return false;
  }
  
  impl_->quic_client_->close_stream(stream_id);
  impl_->stream_manager_->error_stream(stream_id, "Cancelled by user");
  return true;
}

void Client::disconnect() {
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  impl_->authenticated_ = false;
  impl_->quic_client_->disconnect();
}

} // namespace quicftp

