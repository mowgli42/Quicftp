// quicftp_server.h

#ifndef QUICFTP_SERVER_H
#define QUICFTP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include "quic_common.h"
#include "quic_wrapper.h"

namespace quicftp {

class Server {

public:

  Server();
  ~Server();

  // Server lifecycle
  bool start(int port, const std::string& cert_path, const std::string& key_path, 
             const std::string& root_dir = ".");
  
  void stop();
  
  bool is_running() const;

  // Configuration
  void set_verbose(bool verbose);
  bool get_verbose() const;

  // Server root directory for file storage
  void set_root_directory(const std::string& root_dir);
  std::string get_root_directory() const;

  // Event processing (call from main loop)
  void process_events(int timeout_ms = 100);

private:

  // Internal state
  bool running_;
  bool verbose_;
  int port_;
  std::string cert_path_;
  std::string key_path_;
  std::string root_dir_;

  // QUIC server wrapper
  std::unique_ptr<QuicServerWrapper> quic_server_;
  
  // Active connections tracking
  std::map<std::string, std::unique_ptr<QuicConnectionWrapper>> connections_;
  std::mutex connections_mutex_;

  // Verbose logging helpers
  void log_info(const std::string& message) const;
  void log_error(const std::string& message) const;
  void log_connection(const std::string& event, const std::string& client_info) const;
  void log_auth(const std::string& event, const std::string& details) const;
  void log_transfer(const std::string& operation, const std::string& file_path, 
                    size_t file_size, const std::string& status) const;

  // Connection handlers (to be called by QUIC library callbacks)
  void on_client_connect(const std::string& client_address);
  void on_client_disconnect(const std::string& client_address);
  void on_auth_attempt(const std::string& client_address, const std::string& cert_info, bool success);
  
  // File transfer handlers
  bool handle_upload(const std::string& remote_path, const void* data, size_t size);
  bool handle_download(const std::string& remote_path, std::function<bool(const void*, size_t)> send_callback);
  
  // Transfer statistics
  struct TransferStats {
    size_t bytes_transferred;
    std::chrono::steady_clock::time_point start_time;
    double current_speed; // bytes per second
    double average_speed; // bytes per second
  };
  
  mutable std::map<std::string, TransferStats> active_transfers_;
  mutable std::mutex transfers_mutex_;

  // Certificate verification
  bool verify_certificate(const std::string& cert_info);

  // Helper: Get current timestamp for logging
  std::string get_timestamp() const;

  // Helper: Format file size for display
  std::string format_size(size_t bytes) const;
};

} // namespace quicftp

#endif

