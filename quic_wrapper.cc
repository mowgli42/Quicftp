// quic_wrapper.cc
// ngtcp2 implementation

#include "quic_wrapper.h"
#include "quic_common.h"
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <nghttp3/nghttp3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <cstring>

namespace quicftp {

// Helper: Get current timestamp in microseconds
static uint64_t timestamp() {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// ngtcp2 server connection structure
struct ServerConnection {
  ngtcp2_conn* conn;
  ngtcp2_crypto_ossl_ctx* ossl_ctx;
  SSL* ssl;
  int fd;
  std::string address;
  ConnectionState state;
  std::map<StreamId, std::vector<uint8_t>> stream_data;
  std::map<StreamId, std::string> stream_commands;
  
  ServerConnection() : conn(nullptr), ossl_ctx(nullptr), ssl(nullptr), fd(-1), state(ConnectionState::Disconnected) {}
  ~ServerConnection() {
    if (conn) {
      ngtcp2_conn_del(conn);
    }
    if (ssl) {
      SSL_free(ssl);
    }
    if (ossl_ctx) {
      ngtcp2_crypto_ossl_ctx_del(ossl_ctx);
    }
    if (fd >= 0) {
      close(fd);
    }
  }
};

// ngtcp2 server implementation
struct QuicServerImpl {
  int port_;
  int udp_fd_;
  bool listening_;
  std::string cert_path_;
  std::string key_path_;
  SSL_CTX* ssl_ctx_;
  
  ConnectionCallback on_connect_;
  ConnectionCallback on_disconnect_;
  AuthCallback on_auth_;
  std::function<void(StreamId, const std::string&, StreamDataCallback)> on_stream_;
  
  std::map<std::string, std::unique_ptr<ServerConnection>> connections_;
  
  // For backward compatibility with test bridge approach
  std::map<StreamId, std::vector<uint8_t>> stream_data_;
  std::map<StreamId, std::string> stream_commands_;
};

// ngtcp2 client connection structure  
struct ClientConnection {
  ngtcp2_conn* conn;
  ngtcp2_crypto_ossl_ctx* ossl_ctx;
  SSL* ssl;
  int fd;
  std::string server_address;
  ConnectionState state;
  
  ClientConnection() : conn(nullptr), ossl_ctx(nullptr), ssl(nullptr), fd(-1), state(ConnectionState::Disconnected) {}
  ~ClientConnection() {
    if (conn) {
      ngtcp2_conn_del(conn);
    }
    if (ssl) {
      SSL_free(ssl);
    }
    if (ossl_ctx) {
      ngtcp2_crypto_ossl_ctx_del(ossl_ctx);
    }
    if (fd >= 0) {
      close(fd);
    }
  }
};

struct QuicConnectionImpl {
  std::string address_;
  ConnectionState state_;
  ServerConnection* server_conn_;
  
  QuicConnectionImpl() : state_(ConnectionState::Disconnected), server_conn_(nullptr) {}
};

struct QuicStreamImpl {
  StreamId id_;
  StreamState state_;
  ServerConnection* server_conn_;
  
  QuicStreamImpl() : id_(0), state_(StreamState::Idle), server_conn_(nullptr) {}
};

// ngtcp2 callbacks
static int server_recv_stream_data(ngtcp2_conn *conn, uint32_t flags,
                                   int64_t stream_id, uint64_t offset,
                                   const uint8_t *data, size_t datalen,
                                   void *user_data, void *stream_user_data) {
  QuicServerImpl* server = static_cast<QuicServerImpl*>(user_data);
  
  // Find connection
  std::string conn_key;
  for (const auto& pair : server->connections_) {
    if (pair.second->conn == conn) {
      conn_key = pair.first;
      break;
    }
  }
  
  if (conn_key.empty()) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  
  ServerConnection* server_conn = server->connections_[conn_key].get();
  
  // Accumulate stream data
  if (datalen > 0) {
    server_conn->stream_data[stream_id].insert(
      server_conn->stream_data[stream_id].end(),
      data, data + datalen
    );
  }
  
  // If stream is finished, trigger callback
  if (flags & NGTCP2_STREAM_DATA_FLAG_FIN && server->on_stream_) {
    if (server_conn->stream_data.find(stream_id) != server_conn->stream_data.end()) {
      const auto& stream_data = server_conn->stream_data[stream_id];
      StreamDataCallback callback = [](const uint8_t* data, size_t len) { return true; };
      server->on_stream_(stream_id, "", callback);
    }
  }
  
  return 0;
}

static int server_stream_open(ngtcp2_conn *conn, int64_t stream_id, void *user_data) {
  // Stream opened
  return 0;
}

QuicServerWrapper::QuicServerWrapper() : impl_(std::make_unique<QuicServerImpl>()) {
  impl_->listening_ = false;
  impl_->port_ = 0;
  impl_->udp_fd_ = -1;
  impl_->ssl_ctx_ = nullptr;
}

QuicServerWrapper::~QuicServerWrapper() {
  stop();
  if (impl_->ssl_ctx_) {
    SSL_CTX_free(impl_->ssl_ctx_);
  }
}

bool QuicServerWrapper::initialize(int port, const std::string& cert_path, const std::string& key_path) {
  impl_->port_ = port;
  impl_->cert_path_ = cert_path;
  impl_->key_path_ = key_path;
  
  // Initialize OpenSSL
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  
  // Create SSL context
  impl_->ssl_ctx_ = SSL_CTX_new(TLS_server_method());
  if (!impl_->ssl_ctx_) {
    std::cerr << "Failed to create SSL context" << std::endl;
    return false;
  }
  
  // Initialize ngtcp2 crypto
  ngtcp2_crypto_ossl_init();
  
  // Load certificate and key
  if (SSL_CTX_use_certificate_file(impl_->ssl_ctx_, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "Failed to load certificate: " << cert_path << std::endl;
    SSL_CTX_free(impl_->ssl_ctx_);
    impl_->ssl_ctx_ = nullptr;
    return false;
  }
  
  if (SSL_CTX_use_PrivateKey_file(impl_->ssl_ctx_, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "Failed to load private key: " << key_path << std::endl;
    SSL_CTX_free(impl_->ssl_ctx_);
    impl_->ssl_ctx_ = nullptr;
    return false;
  }
  
  return true;
}

bool QuicServerWrapper::start_listening() {
  if (impl_->listening_) {
    return true;
  }
  
  // Create UDP socket
  impl_->udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (impl_->udp_fd_ < 0) {
    std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
    return false;
  }
  
  // Set socket to non-blocking
  int flags = fcntl(impl_->udp_fd_, F_GETFL, 0);
  fcntl(impl_->udp_fd_, F_SETFL, flags | O_NONBLOCK);
  
  // Bind to port
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(impl_->port_);
  
  if (bind(impl_->udp_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    std::cerr << "Failed to bind to port " << impl_->port_ << ": " << strerror(errno) << std::endl;
    close(impl_->udp_fd_);
    impl_->udp_fd_ = -1;
    return false;
  }
  
  impl_->listening_ = true;
  return true;
}

void QuicServerWrapper::stop() {
  impl_->listening_ = false;
  impl_->connections_.clear();
  
  if (impl_->udp_fd_ >= 0) {
    close(impl_->udp_fd_);
    impl_->udp_fd_ = -1;
  }
}

bool QuicServerWrapper::is_listening() const {
  return impl_->listening_;
}

// Helper function to create server connection from initial packet
static ServerConnection* create_server_connection(
    QuicServerImpl* server,
    const uint8_t* pkt,
    size_t pktlen,
    const struct sockaddr_in* client_addr,
    const std::string& conn_key) {
  
  // Parse packet header to extract connection ID
  ngtcp2_version_cid vc;
  
  // Try to parse as version negotiation or initial packet
  int rv = ngtcp2_pkt_decode_version_cid(&vc, pkt, pktlen, NGTCP2_MAX_CIDLEN);
  if (rv < 0) {
    // Not a valid QUIC packet
    return nullptr;
  }
  
  // Check if it's an initial packet (version 1 or 0 for version negotiation)
  uint32_t version = vc.version;
  if (version != NGTCP2_PROTO_VER_V1 && version != 0) {
    // Version negotiation needed (not implemented yet)
    return nullptr;
  }
  
  // Extract destination connection ID from vc.dcid pointer
  ngtcp2_cid dcid;
  if (vc.dcidlen > 0 && vc.dcid) {
    ngtcp2_cid_init(&dcid, vc.dcid, vc.dcidlen);
  } else {
    // Invalid connection ID
    return nullptr;
  }
  
  // Create SSL session for this connection
  SSL* ssl = SSL_new(server->ssl_ctx_);
  if (!ssl) {
    std::cerr << "Failed to create SSL session" << std::endl;
    return nullptr;
  }
  
  // Configure SSL for QUIC server
  ngtcp2_crypto_ossl_configure_server_session(ssl);
  SSL_set_accept_state(ssl);
  
  // Create ngtcp2 crypto context
  ngtcp2_crypto_ossl_ctx* ossl_ctx = nullptr;
  if (ngtcp2_crypto_ossl_ctx_new(&ossl_ctx, ssl) != 0) {
    std::cerr << "Failed to create ngtcp2 crypto context" << std::endl;
    SSL_free(ssl);
    return nullptr;
  }
  
  // Set up path
  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);
  ngtcp2_path* path = &ps.path;
  
  struct sockaddr_in local_addr;
  socklen_t local_len = sizeof(local_addr);
  if (getsockname(server->udp_fd_, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
  }
  
  path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
  path->local.addrlen = sizeof(local_addr);
  path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(const_cast<struct sockaddr_in*>(client_addr));
  path->remote.addrlen = sizeof(*client_addr);
  
  // Generate server connection ID
  ngtcp2_cid scid;
  uint8_t scid_buf[NGTCP2_MAX_CIDLEN];
  if (RAND_bytes(scid_buf, NGTCP2_MAX_CIDLEN) != 1) {
    std::cerr << "Failed to generate server connection ID" << std::endl;
    ngtcp2_crypto_ossl_ctx_del(ossl_ctx);
    SSL_free(ssl);
    return nullptr;
  }
  ngtcp2_cid_init(&scid, scid_buf, NGTCP2_MAX_CIDLEN);
  
  // Set up callbacks
  ngtcp2_callbacks callbacks = {};
  callbacks.recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
  callbacks.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
  callbacks.encrypt = ngtcp2_crypto_encrypt;
  callbacks.decrypt = ngtcp2_crypto_decrypt;
  callbacks.hp_mask = ngtcp2_crypto_hp_mask;
  callbacks.recv_stream_data = server_recv_stream_data;
  callbacks.stream_open = server_stream_open;
  callbacks.rand = [](uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx) -> void {
    RAND_bytes(dest, destlen);
  };
  callbacks.get_new_connection_id = [](ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token,
                                       size_t cidlen, void *user_data) -> int {
    uint8_t buf[NGTCP2_MAX_CIDLEN];
    RAND_bytes(buf, cidlen);
    ngtcp2_cid_init(cid, buf, cidlen);
    return 0;
  };
  
  // Set up settings
  ngtcp2_settings settings;
  ngtcp2_settings_default(&settings);
  settings.log_printf = nullptr;
  
  // Set up transport parameters
  ngtcp2_transport_params params;
  ngtcp2_transport_params_default(&params);
  params.initial_max_stream_data_bidi_local = 128 * 1024;
  params.initial_max_stream_data_bidi_remote = 128 * 1024;
  params.initial_max_stream_data_uni = 128 * 1024;
  params.initial_max_data = 1 * 1024 * 1024;
  
  // Create server connection
  ngtcp2_conn* conn = nullptr;
  rv = ngtcp2_conn_server_new(&conn, &scid, &dcid, path, version,
                              &callbacks, &settings, &params, nullptr, server);
  if (rv != 0) {
    std::cerr << "Failed to create server connection: " << ngtcp2_strerror(rv) << std::endl;
    ngtcp2_crypto_ossl_ctx_del(ossl_ctx);
    SSL_free(ssl);
    return nullptr;
  }
  
  // Set TLS native handle
  ngtcp2_conn_set_tls_native_handle(conn, ossl_ctx);
  
  // Create ServerConnection object
  auto server_conn = std::make_unique<ServerConnection>();
  server_conn->conn = conn;
  server_conn->ossl_ctx = ossl_ctx;
  server_conn->ssl = ssl;
  server_conn->fd = server->udp_fd_;
  server_conn->address = conn_key;
  server_conn->state = ConnectionState::Connecting;
  
  ServerConnection* conn_ptr = server_conn.get();
  server->connections_[conn_key] = std::move(server_conn);
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"SERVER-CONN-CREATE\",\"location\":\"quic_wrapper.cc:create_server_connection\",\"message\":\"Server connection created\",\"data\":{\"conn_key\":\"" << conn_key << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  return conn_ptr;
}

// Helper function to send pending packets for a connection
static void send_pending_packets(ServerConnection* conn, const struct sockaddr_in* client_addr) {
  if (!conn || !conn->conn || conn->fd < 0) {
    return;
  }
  
  // Set up path
  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);
  ngtcp2_path* path = &ps.path;
  
  struct sockaddr_in local_addr;
  socklen_t local_len = sizeof(local_addr);
  if (getsockname(conn->fd, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
  }
  
  path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
  path->local.addrlen = sizeof(local_addr);
  path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(const_cast<struct sockaddr_in*>(client_addr));
  path->remote.addrlen = sizeof(*client_addr);
  
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  ngtcp2_tstamp ts = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  
  uint8_t pkt_buf[65536];
  ngtcp2_pkt_info pi;
  
  // Send packets until no more to send
  while (true) {
    ngtcp2_ssize nwrite = ngtcp2_conn_write_pkt(conn->conn, path, &pi, pkt_buf, sizeof(pkt_buf), ts);
    
    if (nwrite < 0) {
      if (nwrite == NGTCP2_ERR_WRITE_MORE) {
        continue;
      }
      break;
    }
    
    if (nwrite == 0) {
      break;
    }
    
    // Send packet
    ssize_t nsent = sendto(conn->fd, pkt_buf, nwrite, 0,
                          reinterpret_cast<const struct sockaddr*>(client_addr),
                          sizeof(*client_addr));
    
    if (nsent < 0) {
      break;
    }
    
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"SERVER-SEND\",\"location\":\"quic_wrapper.cc:send_pending_packets\",\"message\":\"Server sent packet\",\"data\":{\"packet_size\":" << nsent << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
  }
}

void QuicServerWrapper::process_events(int timeout_ms) {
  if (!impl_->listening_ || impl_->udp_fd_ < 0) {
    return;
  }
  
  // Receive UDP packets
  uint8_t buf[65536];
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  
  while (true) {
    ssize_t nread = recvfrom(impl_->udp_fd_, buf, sizeof(buf), 0,
                             (struct sockaddr*)&client_addr, &client_addr_len);
    
    if (nread < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break; // No more data
      }
      std::cerr << "recvfrom error: " << strerror(errno) << std::endl;
      break;
    }
    
    // Create connection key
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
    std::string conn_key = std::string(addr_str) + ":" + std::to_string(ntohs(client_addr.sin_port));
    
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"SERVER-RECV\",\"location\":\"quic_wrapper.cc:process_events\",\"message\":\"Server received packet\",\"data\":{\"packet_size\":" << nread << ",\"conn_key\":\"" << conn_key << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    
    // Find or create connection
    ServerConnection* server_conn = nullptr;
    if (impl_->connections_.find(conn_key) == impl_->connections_.end()) {
      // Create new connection from initial packet
      server_conn = create_server_connection(impl_.get(), buf, nread, &client_addr, conn_key);
      if (!server_conn) {
        // Failed to create connection, skip this packet
        continue;
      }
      
      if (impl_->on_connect_) {
        impl_->on_connect_(conn_key);
      }
    } else {
      server_conn = impl_->connections_[conn_key].get();
    }
    
    if (!server_conn || !server_conn->conn) {
      continue;
    }
    
    // Set up path
    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);
    ngtcp2_path* path = &ps.path;
    
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(impl_->udp_fd_, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
      memset(&local_addr, 0, sizeof(local_addr));
      local_addr.sin_family = AF_INET;
      local_addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
    path->local.addrlen = sizeof(local_addr);
    path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&client_addr);
    path->remote.addrlen = sizeof(client_addr);
    
    // Process packet
    ngtcp2_pkt_info pi;
    pi.ecn = NGTCP2_ECN_NOT_ECT;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    ngtcp2_tstamp ts = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    
    ngtcp2_ssize nread_processed = ngtcp2_conn_read_pkt(server_conn->conn, path, &pi, buf, nread, ts);
    
    if (nread_processed < 0) {
      std::cerr << "Failed to process packet: " << ngtcp2_strerror(nread_processed) << std::endl;
      continue;
    }
    
    // Check if handshake is complete
    if (server_conn->ssl && SSL_is_init_finished(server_conn->ssl)) {
      if (server_conn->state == ConnectionState::Connecting) {
        server_conn->state = ConnectionState::Connected;
        // #region agent log
        {
          std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
          if (log_file.is_open()) {
            log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"SERVER-HANDSHAKE-DONE\",\"location\":\"quic_wrapper.cc:process_events\",\"message\":\"Server handshake completed\",\"data\":{\"conn_key\":\"" << conn_key << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
            log_file.close();
          }
        }
        // #endregion
      }
    }
    
    // Send any pending packets (handshake responses)
    send_pending_packets(server_conn, &client_addr);
  }
  
  // Process completed uploads from connections
  for (auto& pair : impl_->connections_) {
    auto& conn = pair.second;
    for (auto it = conn->stream_data.begin(); it != conn->stream_data.end();) {
      StreamId sid = it->first;
      if (!it->second.empty()) {
        // Move to global tracking for get_pending_uploads()
        impl_->stream_data_[sid] = std::move(it->second);
        impl_->stream_commands_[sid] = ""; // Will be set by stream callback
        it = conn->stream_data.erase(it);
      } else {
        ++it;
      }
    }
  }
  
  // Sleep to prevent busy waiting
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
    
    if (impl_->stream_data_.find(sid) != impl_->stream_data_.end() && 
        !impl_->stream_data_[sid].empty()) {
      PendingUpload upload;
      upload.stream_id = sid;
      upload.remote_path = it->second;
      upload.data = std::move(impl_->stream_data_[sid]);
      uploads.push_back(upload);
      
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
  // TODO: Send data over QUIC stream using ngtcp2
  return true;
}

void QuicStreamWrapper::close() {
  impl_->state_ = StreamState::Closed;
}

} // namespace quicftp
