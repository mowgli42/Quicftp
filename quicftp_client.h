// quicftp_client.h

#ifndef QUICFTP_CLIENT_H
#define QUICFTP_CLIENT_H

#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <functional>
#include "quic_common.h"

namespace quicftp {

class Client {

public:

  Client();
  ~Client();

  bool connect(const std::string& server);
  
  bool authenticate(const std::string& cert_path);

  bool login(const std::string& username, const std::string& password);

  bool upload_file(const std::string& local_path, const std::string& remote_path);

  bool download_file(const std::string& remote_path, const std::string& local_path);

  // Parallel transfer methods
  bool upload_files(const std::vector<std::pair<std::string, std::string>>& files); // (local, remote) pairs
  bool download_files(const std::vector<std::pair<std::string, std::string>>& files); // (remote, local) pairs

  // Progress and cancellation
  void set_progress_callback(std::function<void(StreamId, size_t, size_t)> callback);
  bool cancel_transfer(StreamId stream_id);

  bool logout();

  void disconnect();

private:

  // PIMPL idiom for implementation details
  class Impl;
  std::unique_ptr<Impl> impl_;

};

} // namespace quicftp

#endif