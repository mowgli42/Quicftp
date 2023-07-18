// quicftp_client.h

#ifndef QUICFTP_CLIENT_H
#define QUICFTP_CLIENT_H

#include <string>

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

  bool logout();

  void disconnect();

private:

  // Internals for connection, streams, 
  // parsing commands, file transfer etc.

};

} // namespace quicftp

#endif