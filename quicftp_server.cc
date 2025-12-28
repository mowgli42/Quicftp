// quicftp_server.cc

#include "quicftp_server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <vector>
#include <cstring>

namespace quicftp {

Server::Server() 
  : running_(false)
  , verbose_(true)
  , port_(0)
  , quic_server_(nullptr)
{
}

Server::~Server() {
  if (running_) {
    stop();
  }
}

bool Server::start(int port, const std::string& cert_path, const std::string& key_path,
                   const std::string& root_dir) {
  if (running_) {
    log_error("Server is already running");
    return false;
  }

  port_ = port;
  cert_path_ = cert_path;
  key_path_ = key_path;
  root_dir_ = root_dir.empty() ? "." : root_dir;

  // Validate certificate and key files exist
  std::ifstream cert_file(cert_path);
  if (!cert_file.good()) {
    log_error("Certificate file not found: " + cert_path);
    return false;
  }
  cert_file.close();

  std::ifstream key_file(key_path);
  if (!key_file.good()) {
    log_error("Key file not found: " + key_path);
    return false;
  }
  key_file.close();

  // Create root directory if it doesn't exist
  try {
    std::filesystem::create_directories(root_dir_);
  } catch (const std::exception& e) {
    log_error("Failed to create root directory: " + std::string(e.what()));
    return false;
  }

  // Initialize QUIC server
  quic_server_ = std::make_unique<QuicServerWrapper>();
  if (!quic_server_->initialize(port_, cert_path_, key_path_)) {
    log_error("Failed to initialize QUIC server");
    return false;
  }

  // Set up callbacks
  quic_server_->set_connection_callback(
    [this](const std::string& addr) { this->on_client_connect(addr); },
    [this](const std::string& addr) { this->on_client_disconnect(addr); }
  );
  quic_server_->set_auth_callback(
    [this](const std::string& addr, const std::string& cert, bool success) {
      this->on_auth_attempt(addr, cert, success);
    }
  );

  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H\",\"location\":\"quicftp_server.cc:81\",\"message\":\"Server callbacks configured\",\"data\":{\"root_dir\":\"" << root_dir_ << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion

  running_ = true;

  log_info("Server starting on port " + std::to_string(port_));
  log_info("Certificate: " + cert_path_);
  log_info("Key: " + key_path_);
  log_info("Root directory: " + root_dir_);
  log_info("Server is listening for QUIC connections...");

  // Start QUIC server listening
  if (!quic_server_->start_listening()) {
    log_error("Failed to start QUIC server listening");
    quic_server_.reset();
    return false;
  }

  return true;
}

void Server::stop() {
  if (!running_) {
    return;
  }

  log_info("Server stopping...");

  // Stop QUIC server and close all connections
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [addr, conn] : connections_) {
      if (conn) {
        conn->close();
      }
    }
    connections_.clear();
  }

  if (quic_server_) {
    quic_server_->stop();
    quic_server_.reset();
  }

  running_ = false;
  log_info("Server stopped");
}

bool Server::is_running() const {
  return running_;
}

void Server::set_verbose(bool verbose) {
  verbose_ = verbose;
}

bool Server::get_verbose() const {
  return verbose_;
}

void Server::set_root_directory(const std::string& root_dir) {
  if (!running_) {
    root_dir_ = root_dir.empty() ? "." : root_dir;
  }
}

std::string Server::get_root_directory() const {
  return root_dir_;
}

void Server::process_events(int timeout_ms) {
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"L\",\"location\":\"quicftp_server.cc:155\",\"message\":\"process_events() called\",\"data\":{\"timeout_ms\":" << timeout_ms << ",\"running\":" << (running_ ? "true" : "false") << ",\"quic_server_exists\":" << (quic_server_ ? "true" : "false") << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  if (quic_server_ && running_) {
    quic_server_->process_events(timeout_ms);
    
    // Process completed uploads (test mode)
    auto uploads = quic_server_->get_pending_uploads();
    
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"L\",\"location\":\"quicftp_server.cc:168\",\"message\":\"Checking for pending uploads\",\"data\":{\"upload_count\":" << uploads.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    
    for (const auto& upload : uploads) {
      // #region agent log
      {
        std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
        if (log_file.is_open()) {
          log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"J\",\"location\":\"quicftp_server.cc:157\",\"message\":\"Processing pending upload\",\"data\":{\"stream_id\":" << upload.stream_id << ",\"remote_path\":\"" << upload.remote_path << "\",\"data_size\":" << upload.data.size() << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
          log_file.close();
        }
      }
      // #endregion
      
      handle_upload(upload.remote_path, upload.data.data(), upload.data.size());
    }
  }
}

void Server::log_info(const std::string& message) const {
  if (verbose_) {
    std::cout << "[" << get_timestamp() << "] [INFO] " << message << std::endl;
  }
}

void Server::log_error(const std::string& message) const {
  std::cerr << "[" << get_timestamp() << "] [ERROR] " << message << std::endl;
}

void Server::log_connection(const std::string& event, const std::string& client_info) const {
  if (verbose_) {
    std::cout << "[" << get_timestamp() << "] [CONNECTION] " << event 
              << " - Client: " << client_info << std::endl;
  }
}

void Server::log_auth(const std::string& event, const std::string& details) const {
  if (verbose_) {
    std::cout << "[" << get_timestamp() << "] [AUTH] " << event 
              << " - " << details << std::endl;
  }
}

void Server::log_transfer(const std::string& operation, const std::string& file_path,
                          size_t file_size, const std::string& status) const {
  if (verbose_) {
    std::cout << "[" << get_timestamp() << "] [TRANSFER] " << operation 
              << " - File: " << file_path 
              << " (" << format_size(file_size) << ") - " << status << std::endl;
  }
}

void Server::on_client_connect(const std::string& client_address) {
  log_connection("Client connected", client_address);
  std::lock_guard<std::mutex> lock(connections_mutex_);
  connections_[client_address] = std::make_unique<QuicConnectionWrapper>();
}

void Server::on_client_disconnect(const std::string& client_address) {
  log_connection("Client disconnected", client_address);
  std::lock_guard<std::mutex> lock(connections_mutex_);
  connections_.erase(client_address);
}

void Server::on_auth_attempt(const std::string& client_address, const std::string& cert_info, bool success) {
  if (success) {
    log_auth("Authentication successful", "Client: " + client_address + ", Cert: " + cert_info);
  } else {
    log_auth("Authentication failed", "Client: " + client_address + ", Cert: " + cert_info);
  }
}

bool Server::handle_upload(const std::string& remote_path, const void* data, size_t size) {
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"G\",\"location\":\"quicftp_server.cc:208\",\"message\":\"handle_upload() called\",\"data\":{\"remote_path\":\"" << remote_path << "\",\"size\":" << size << ",\"root_dir\":\"" << root_dir_ << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  std::string full_path = root_dir_ + "/" + remote_path;
  
  // Security: Prevent directory traversal
  std::filesystem::path safe_path = std::filesystem::canonical(root_dir_) / remote_path;
  std::filesystem::path root_canonical = std::filesystem::canonical(root_dir_);
  
  if (safe_path.string().find(root_canonical.string()) != 0) {
    log_error("Upload rejected: Path traversal attempt - " + remote_path);
    return false;
  }

  auto start_time = std::chrono::steady_clock::now();
  
  try {
    // Create parent directories if needed
    std::filesystem::create_directories(safe_path.parent_path());

    std::ofstream file(safe_path, std::ios::binary);
    if (!file.is_open()) {
      log_error("Upload failed: Cannot open file for writing - " + full_path);
      return false;
    }

    log_transfer("Upload", remote_path, size, "Starting");

    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"G\",\"location\":\"quicftp_server.cc:234\",\"message\":\"About to write file\",\"data\":{\"safe_path\":\"" << safe_path.string() << "\",\"size\":" << size << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion

    file.write(static_cast<const char*>(data), size);
    if (!file.good()) {
      log_error("Upload failed: Write error - " + full_path);
      return false;
    }

    file.close();
    
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        bool file_exists = std::filesystem::exists(safe_path);
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"G\",\"location\":\"quicftp_server.cc:242\",\"message\":\"File write completed\",\"data\":{\"safe_path\":\"" << safe_path.string() << "\",\"file_exists\":" << (file_exists ? "true" : "false") << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    
    // Calculate transfer speed
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double speed = (duration > 0) ? (static_cast<double>(size) / duration) * 1000.0 : 0.0; // bytes per second
    
    std::ostringstream status;
    status << "Completed - Speed: " << format_size(static_cast<size_t>(speed)) << "/s";
    log_transfer("Upload", remote_path, size, status.str());
    
    return true;

  } catch (const std::exception& e) {
    log_error("Upload failed: " + std::string(e.what()) + " - " + full_path);
    return false;
  }
}

bool Server::handle_download(const std::string& remote_path, 
                            std::function<bool(const void*, size_t)> send_callback) {
  std::filesystem::path safe_path = std::filesystem::canonical(root_dir_) / remote_path;
  std::filesystem::path root_canonical = std::filesystem::canonical(root_dir_);
  
  // Security: Prevent directory traversal
  if (safe_path.string().find(root_canonical.string()) != 0) {
    log_error("Download rejected: Path traversal attempt - " + remote_path);
    return false;
  }

  if (!std::filesystem::exists(safe_path)) {
    log_error("Download failed: File not found - " + remote_path);
    return false;
  }

  if (!std::filesystem::is_regular_file(safe_path)) {
    log_error("Download failed: Not a regular file - " + remote_path);
    return false;
  }

  try {
    std::ifstream file(safe_path, std::ios::binary);
    if (!file.is_open()) {
      log_error("Download failed: Cannot open file for reading - " + remote_path);
      return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto start_time = std::chrono::steady_clock::now();
    log_transfer("Download", remote_path, file_size, "Starting");

    // Read and send file in chunks
    const size_t chunk_size = 64 * 1024; // 64KB chunks
    std::vector<char> buffer(chunk_size);
    size_t total_sent = 0;

    while (file.read(buffer.data(), chunk_size) || file.gcount() > 0) {
      size_t bytes_read = file.gcount();
      if (!send_callback(buffer.data(), bytes_read)) {
        log_error("Download failed: Send callback returned false - " + remote_path);
        return false;
      }
      total_sent += bytes_read;
    }

    if (!file.eof() && file.fail()) {
      log_error("Download failed: Read error - " + remote_path);
      return false;
    }

    // Calculate transfer speed
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double speed = (duration > 0) ? (static_cast<double>(file_size) / duration) * 1000.0 : 0.0; // bytes per second
    
    std::ostringstream status;
    status << "Completed - Speed: " << format_size(static_cast<size_t>(speed)) << "/s";
    log_transfer("Download", remote_path, file_size, status.str());
    
    return true;

  } catch (const std::exception& e) {
    log_error("Download failed: " + std::string(e.what()) + " - " + remote_path);
    return false;
  }
}

bool Server::verify_certificate(const std::string& cert_info) {
  // TODO: Implement actual certificate verification using OpenSSL
  // This should verify the client's certificate against the server's trust store
  // For now, we'll do a basic placeholder that logs the attempt
  log_auth("Certificate verification", "Cert info: " + cert_info);
  
  // Basic validation: check if cert_info is not empty
  if (cert_info.empty()) {
    log_auth("Certificate verification failed", "Empty certificate");
    return false;
  }
  
  // TODO: Use OpenSSL to verify certificate chain, expiration, etc.
  return true; // Placeholder - will be replaced with actual verification
}

std::string Server::get_timestamp() const {
  auto now = std::time(nullptr);
  auto tm = *std::localtime(&now);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string Server::format_size(size_t bytes) const {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  size_t unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unit_index < 4) {
    size /= 1024.0;
    unit_index++;
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
  return oss.str();
}

} // namespace quicftp

