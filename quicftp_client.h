// quicftp_client.h

#ifndef QUICFTP_CLIENT_H
#define QUICFTP_CLIENT_H

#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <functional>

namespace quicftp {

// Callback type for progress updates: (file_path, bytes_transferred, total_bytes)
using ProgressCallback = std::function<void(const std::string&, size_t, size_t)>;

class Client {

public:

  Client();
  ~Client();

  // Configure connection (server URL like "https://127.0.0.1:443")
  bool connect(const std::string& server_url);
  
  // Configure authentication (client certificate path)
  bool authenticate(const std::string& cert_path, const std::string& key_path = "");

  // Single file operations
  bool upload_file(const std::string& local_path, const std::string& remote_path);
  bool download_file(const std::string& remote_path, const std::string& local_path);

  // Parallel transfer methods
  bool upload_files(const std::vector<std::pair<std::string, std::string>>& files); // (local, remote) pairs
  bool download_files(const std::vector<std::pair<std::string, std::string>>& files); // (remote, local) pairs

  // Configuration
  void set_progress_callback(ProgressCallback callback);
  void set_verbose(bool verbose);

private:

  class Impl;
  std::unique_ptr<Impl> impl_;

};

} // namespace quicftp

#endif
