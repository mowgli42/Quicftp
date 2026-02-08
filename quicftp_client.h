// quicftp_client.h

#ifndef QUICFTP_CLIENT_H
#define QUICFTP_CLIENT_H

#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <functional>
#include <cstdint>

namespace quicftp {

// ============================================================================
// Error handling types
// ============================================================================

enum class ErrorCode {
  Ok = 0,
  ConnectionFailed,
  AuthenticationFailed,
  FileNotFound,
  FileOpenError,
  UploadFailed,
  DownloadFailed,
  ServerError,
  Timeout,
  SSLError,
  NotConnected,
  InvalidArgument,
  Unknown
};

// Convert error code to human-readable string
const char* error_code_str(ErrorCode code);

// Structured result for transfer operations
struct TransferResult {
  bool success = false;
  ErrorCode error = ErrorCode::Ok;
  std::string message;       // Human-readable error message
  long http_status = 0;      // HTTP status code (200, 404, 500, etc.)
  size_t bytes_transferred = 0;

  // Convenience: implicit bool conversion
  explicit operator bool() const { return success; }

  // Factory helpers
  static TransferResult ok(size_t bytes = 0);
  static TransferResult fail(ErrorCode code, const std::string& msg, long http_status = 0);
};

// Result for parallel (batch) transfer operations
struct BatchTransferResult {
  bool all_success = false;
  std::vector<std::pair<std::string, TransferResult>> results; // (file_path, result) pairs
  size_t succeeded_count = 0;
  size_t failed_count = 0;

  explicit operator bool() const { return all_success; }
};

// ============================================================================
// Progress callback
// ============================================================================

// Callback type for progress updates: (file_path, bytes_transferred, total_bytes)
using ProgressCallback = std::function<void(const std::string&, size_t, size_t)>;

// ============================================================================
// Client class
// ============================================================================

class Client {

public:

  Client();
  ~Client();

  // Configure connection (server URL like "https://localhost:443")
  bool connect(const std::string& server_url);
  
  // Configure authentication (client certificate path)
  bool authenticate(const std::string& cert_path, const std::string& key_path = "");

  // --------------------------------------------------------------------------
  // Single file operations (structured results)
  // --------------------------------------------------------------------------
  TransferResult upload(const std::string& local_path, const std::string& remote_path);
  TransferResult download(const std::string& remote_path, const std::string& local_path);

  // --------------------------------------------------------------------------
  // Parallel transfer methods (structured results)
  // --------------------------------------------------------------------------
  BatchTransferResult upload_batch(const std::vector<std::pair<std::string, std::string>>& files);
  BatchTransferResult download_batch(const std::vector<std::pair<std::string, std::string>>& files);

  // --------------------------------------------------------------------------
  // Legacy bool API (preserved for backward compatibility)
  // --------------------------------------------------------------------------
  bool upload_file(const std::string& local_path, const std::string& remote_path);
  bool download_file(const std::string& remote_path, const std::string& local_path);
  bool upload_files(const std::vector<std::pair<std::string, std::string>>& files);
  bool download_files(const std::vector<std::pair<std::string, std::string>>& files);

  // --------------------------------------------------------------------------
  // Configuration
  // --------------------------------------------------------------------------
  void set_progress_callback(ProgressCallback callback);
  void set_verbose(bool verbose);

  // TLS configuration
  void set_ca_cert(const std::string& ca_path);
  void set_insecure(bool insecure);

  // Transfer options
  void set_bandwidth_limit(size_t upload_bps, size_t download_bps);

private:

  class Impl;
  std::unique_ptr<Impl> impl_;

};

} // namespace quicftp

#endif
