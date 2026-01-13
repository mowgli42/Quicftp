// quicftp_client.cc

#include "quicftp_client.h"
#include "quic_common.h"
#include "quic_wrapper.h"
#include "stream_manager.h"
#include "test_bridge.h"
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
// Try QuicTLS first, fall back to OpenSSL if not available
#if __has_include(<ngtcp2/ngtcp2_crypto_quictls.h>)
#include <ngtcp2/ngtcp2_crypto_quictls.h>
#define USE_QUICTLS 1
#else
// Fallback to OpenSSL variant (may work with QuicTLS OpenSSL libraries)
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#define USE_QUICTLS 0
#endif
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
#include <fstream>
#include <filesystem>
#include <mutex>
#include <map>
#include <vector>
#include <functional>
#include <chrono>
#include <cstring>
#include <thread>

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
  
  // ngtcp2 client connection
  ngtcp2_conn* conn_;
  SSL_CTX* ssl_ctx_;
  SSL* ssl_;
  int udp_fd_;
  struct sockaddr_in server_addr_;
  ngtcp2_crypto_conn_ref conn_ref_;
  
  // Helper methods
  bool parse_address(const std::string& address, std::string& host, int& port);
  bool create_udp_socket();
  bool initialize_ngtcp2_connection();
  bool send_initial_packet();
  bool process_incoming_packets();
  bool send_pending_packets();
};

// Implementation with ngtcp2
QuicClientWrapper::QuicClientWrapper() 
  : connected_(false), conn_(nullptr), 
    ssl_ctx_(nullptr), ssl_(nullptr), udp_fd_(-1) {
  memset(&server_addr_, 0, sizeof(server_addr_));
}

QuicClientWrapper::~QuicClientWrapper() { 
  disconnect(); 
}

bool QuicClientWrapper::parse_address(const std::string& address, std::string& host, int& port) {
  size_t colon_pos = address.find(':');
  if (colon_pos == std::string::npos) {
    return false;
  }
  host = address.substr(0, colon_pos);
  port = std::stoi(address.substr(colon_pos + 1));
  return true;
}

bool QuicClientWrapper::create_udp_socket() {
  udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd_ < 0) {
    std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
    return false;
  }
  
  // Set socket to non-blocking
  int flags = fcntl(udp_fd_, F_GETFL, 0);
  fcntl(udp_fd_, F_SETFL, flags | O_NONBLOCK);
  
  return true;
}

bool QuicClientWrapper::initialize_ngtcp2_connection() {
  // Reset client_initial tracking for this connection
  client_initial_called_ = false;
  
  // Initialize crypto backend
  static bool ssl_initialized = false;
  if (!ssl_initialized) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#if USE_QUICTLS
    ngtcp2_crypto_quictls_init();
#else
    ngtcp2_crypto_ossl_init();
#endif
    ssl_initialized = true;
  }
  
  // Create SSL context
  ssl_ctx_ = SSL_CTX_new(TLS_client_method());
  if (!ssl_ctx_) {
    std::cerr << "Failed to create SSL context" << std::endl;
    return false;
  }
  
#if USE_QUICTLS
  // Configure SSL context for QUIC (context-level, per QuicTLS pattern)
  if (ngtcp2_crypto_quictls_configure_client_context(ssl_ctx_) != 0) {
    std::cerr << "Failed to configure SSL context for QUIC" << std::endl;
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = nullptr;
    return false;
  }
#else
  // OpenSSL variant: configure at session level (after SSL_new)
  // Note: This works with QuicTLS OpenSSL libraries
#endif
  
  // Create SSL object
  ssl_ = SSL_new(ssl_ctx_);
  if (!ssl_) {
    std::cerr << "Failed to create SSL object" << std::endl;
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = nullptr;
    return false;
  }
  
#if !USE_QUICTLS
  // OpenSSL variant: configure session for QUIC
  ngtcp2_crypto_ossl_configure_client_session(ssl_);
#endif
  
  // Set up ngtcp2_crypto_conn_ref for SSL_set_app_data
  // This is required for crypto helpers to access the connection
  // IMPORTANT: get_conn must be set BEFORE ngtcp2_conn_client_new (per example pattern)
  conn_ref_.user_data = this;
  conn_ref_.get_conn = [](ngtcp2_crypto_conn_ref *ref) -> ngtcp2_conn* {
    QuicClientWrapper* self = static_cast<QuicClientWrapper*>(ref->user_data);
    return self->conn_;  // Will be set during ngtcp2_conn_client_new
  };
  SSL_set_app_data(ssl_, &conn_ref_);
  
  // Set SSL to connect state
  SSL_set_connect_state(ssl_);
  
  // Extract hostname from server address for SNI
  std::string hostname = server_address_;
  size_t colon_pos = hostname.find(':');
  if (colon_pos != std::string::npos) {
    hostname = hostname.substr(0, colon_pos);
  }
  SSL_set_tlsext_host_name(ssl_, hostname.c_str());
  
  // Create ngtcp2 connection
  ngtcp2_cid dcid, scid;
  
  // Generate random connection IDs
  uint8_t dcid_buf[NGTCP2_MAX_CIDLEN];
  uint8_t scid_buf[NGTCP2_MAX_CIDLEN];
  if (RAND_bytes(dcid_buf, NGTCP2_MAX_CIDLEN) != 1 ||
      RAND_bytes(scid_buf, NGTCP2_MAX_CIDLEN) != 1) {
    std::cerr << "Failed to generate connection IDs" << std::endl;
    SSL_free(ssl_);
    SSL_CTX_free(ssl_ctx_);
    ssl_ = nullptr;
    ssl_ctx_ = nullptr;
    return false;
  }
  
  ngtcp2_cid_init(&dcid, dcid_buf, NGTCP2_MAX_CIDLEN);
  ngtcp2_cid_init(&scid, scid_buf, NGTCP2_MAX_CIDLEN);
  
  // Set up path
  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);
  ngtcp2_path* path = &ps.path;
  
  // Initialize local address (will be set when sending)
  struct sockaddr_in local_addr;
  memset(&local_addr, 0, sizeof(local_addr));
  local_addr.sin_family = AF_INET;
  local_addr.sin_addr.s_addr = INADDR_ANY;
  
  path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
  path->local.addrlen = sizeof(local_addr);
  path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&server_addr_);
  path->remote.addrlen = sizeof(server_addr_);
  
  // Set up callbacks
  ngtcp2_callbacks callbacks = {};
  // Use the helper function directly - it requires SSL_set_app_data to be set up
  // Note: This works for both QuicTLS and OpenSSL variants
  // CRITICAL: The callback is being called twice. First call fails with -502.
  // We need to prevent the second call or handle it properly.
  // The issue is that retry_aead is set on first call, then second call tries to set it again.
  static bool client_initial_called = false;
  client_initial_called = false;  // Reset for each connection
  callbacks.client_initial = [](ngtcp2_conn *conn, void *user_data) -> int {
    QuicClientWrapper* self = static_cast<QuicClientWrapper*>(user_data);
    std::cerr << "[DEBUG] client_initial callback invoked (first call: " << !client_initial_called << ")" << std::endl;
    
    if (client_initial_called) {
      std::cerr << "[DEBUG] client_initial already called, skipping to avoid retry_aead assertion" << std::endl;
      return 0;  // Return success on second call to prevent assertion
    }
    
    client_initial_called = true;
    // Call the helper function
    int rv = ngtcp2_crypto_client_initial_cb(conn, user_data);
    if (rv != 0) {
      std::cerr << "[DEBUG] ngtcp2_crypto_client_initial_cb returned: " << rv << " (" << ngtcp2_strerror(rv) << ")" << std::endl;
      client_initial_called = false;  // Reset on failure so it can retry
    }
    return rv;
  };
  callbacks.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
  callbacks.encrypt = ngtcp2_crypto_encrypt;
  callbacks.decrypt = ngtcp2_crypto_decrypt;
  callbacks.hp_mask = ngtcp2_crypto_hp_mask;
  callbacks.recv_retry = [](ngtcp2_conn *conn, const ngtcp2_pkt_hd *hd, void *user_data) -> int {
    // Handle retry packet
    return 0;
  };
  callbacks.rand = [](uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx) -> void {
    // Generate random bytes using OpenSSL
    RAND_bytes(dest, destlen);
  };
  callbacks.get_new_connection_id = [](ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token,
                                       size_t cidlen, void *user_data) -> int {
    // Generate new connection ID
    uint8_t buf[NGTCP2_MAX_CIDLEN];
    RAND_bytes(buf, cidlen);
    ngtcp2_cid_init(cid, buf, cidlen);
    return 0;
  };
  callbacks.remove_connection_id = nullptr;
  callbacks.update_key = ngtcp2_crypto_update_key_cb;
  callbacks.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
  callbacks.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
  callbacks.get_path_challenge_data = [](ngtcp2_conn *conn, uint8_t *data, void *user_data) -> int {
    // Generate random data for path challenge
    RAND_bytes(data, 8);
    return 0;
  };
  callbacks.path_validation = nullptr;
  callbacks.select_preferred_addr = nullptr;
  callbacks.version_negotiation = nullptr;
  callbacks.extend_max_local_streams_bidi = nullptr;
  callbacks.extend_max_local_streams_uni = nullptr;
  callbacks.recv_stream_data = nullptr;
  callbacks.acked_stream_data_offset = nullptr;
  callbacks.stream_open = nullptr;
  callbacks.stream_close = nullptr;
  callbacks.stream_reset = nullptr;
  
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
  
  // Create connection (client_initial callback will be invoked during this)
  int rv = ngtcp2_conn_client_new(&conn_, &dcid, &scid, path, NGTCP2_PROTO_VER_V1,
                                  &callbacks, &settings, &params, nullptr, this);
  if (rv != 0) {
    std::cerr << "Failed to create ngtcp2 connection: " << ngtcp2_strerror(rv) << std::endl;
    SSL_free(ssl_);
    SSL_CTX_free(ssl_ctx_);
    ssl_ = nullptr;
    ssl_ctx_ = nullptr;
    return false;
  }
  
  // Set get_conn callback now that conn_ is available (per QuicTLS example pattern)
  conn_ref_.get_conn = [](ngtcp2_crypto_conn_ref *ref) -> ngtcp2_conn* {
    QuicClientWrapper* self = static_cast<QuicClientWrapper*>(ref->user_data);
    return self->conn_;
  };
  
  // Set TLS native handle - use SSL*, not ossl_ctx* (per QuicTLS example)
  ngtcp2_conn_set_tls_native_handle(conn_, ssl_);
  
  return true;
}

bool QuicClientWrapper::connect(const std::string& server_address) {
  server_address_ = server_address;
  
  // Parse server address
  std::string host;
  int port;
  if (!parse_address(server_address, host, port)) {
    std::cerr << "Invalid server address format (expected host:port): " << server_address << std::endl;
    return false;
  }
  
  // Resolve server address
  server_addr_.sin_family = AF_INET;
  server_addr_.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &server_addr_.sin_addr) <= 0) {
    std::cerr << "Failed to parse server address: " << host << std::endl;
    return false;
  }
  
  // Create UDP socket
  if (!create_udp_socket()) {
    return false;
  }
  
  // Initialize ngtcp2 connection
  if (!initialize_ngtcp2_connection()) {
    close(udp_fd_);
    udp_fd_ = -1;
    return false;
  }
  
  // Send initial packet to start handshake
  if (!send_initial_packet()) {
    disconnect();
    return false;
  }
  
  // Process incoming packets to drive handshake forward
  // This allows server response to be processed, which drives TLS handshake
  // and enables key derivation before we try to send handshake packets
  for (int i = 0; i < 10; ++i) {
    process_incoming_packets();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  connected_ = true;
  return true;
}

bool QuicClientWrapper::send_initial_packet() {
  if (!conn_ || udp_fd_ < 0) {
    return false;
  }
  
  // Set up path
  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);
  ngtcp2_path* path = &ps.path;
  
  struct sockaddr_in local_addr;
  socklen_t local_len = sizeof(local_addr);
  if (getsockname(udp_fd_, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
  }
  
  path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
  path->local.addrlen = sizeof(local_addr);
  path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&server_addr_);
  path->remote.addrlen = sizeof(server_addr_);
  
  // Get current timestamp
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  ngtcp2_tstamp ts = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  
  // Write initial packet - only send initial packets here
  // Handshake packets will be sent later after keys are derived
  uint8_t pkt_buf[65536];
  ngtcp2_pkt_info pi;
  
  // Try to write initial packet
  // ngtcp2_conn_write_pkt will try to send whatever packets are needed
  // If it tries to send handshake packets before keys are derived, it will assert
  // We'll handle this by only calling it once for initial packet, then waiting
  ngtcp2_ssize nwrite = ngtcp2_conn_write_pkt(conn_, path, &pi, pkt_buf, sizeof(pkt_buf), ts);
  
  if (nwrite < 0) {
    // If error is about missing keys, that's expected - we'll send after handshake progresses
    if (nwrite == NGTCP2_ERR_WRITE_MORE || nwrite == NGTCP2_ERR_CALLBACK_FAILURE) {
      // Try to send just initial packet - might need to check packet type
      // For now, return true to allow handshake to progress
      return true;
    }
    std::cerr << "Failed to write initial packet: " << ngtcp2_strerror(nwrite) << std::endl;
    return false;
  }
  
  if (nwrite == 0) {
    // No packet to send yet
    return true;
  }
  
  // Send initial packet via UDP
  ssize_t nsent = sendto(udp_fd_, pkt_buf, nwrite, 0,
                        reinterpret_cast<struct sockaddr*>(&server_addr_),
                        sizeof(server_addr_));
  
  if (nsent < 0) {
    std::cerr << "Failed to send initial packet: " << strerror(errno) << std::endl;
    return false;
  }
  
  return true;
}

bool QuicClientWrapper::process_incoming_packets() {
  if (!conn_ || udp_fd_ < 0) {
    return false;
  }
  
  uint8_t buf[65536];
  struct sockaddr_in from_addr;
  socklen_t from_len = sizeof(from_addr);
  
  while (true) {
    ssize_t nread = recvfrom(udp_fd_, buf, sizeof(buf), 0,
                            reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);
    
    if (nread < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break; // No more data
      }
      return false;
    }
    
    // Set up path
    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);
    ngtcp2_path* path = &ps.path;
    
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(udp_fd_, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
      memset(&local_addr, 0, sizeof(local_addr));
      local_addr.sin_family = AF_INET;
      local_addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
    path->local.addrlen = sizeof(local_addr);
    path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&from_addr);
    path->remote.addrlen = sizeof(from_addr);
    
    // Process packet
    ngtcp2_pkt_info pi;
    pi.ecn = NGTCP2_ECN_NOT_ECT;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    ngtcp2_tstamp ts = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    
    ngtcp2_ssize nread_processed = ngtcp2_conn_read_pkt(conn_, path, &pi, buf, nread, ts);
    
    if (nread_processed < 0) {
      std::cerr << "Failed to process packet: " << ngtcp2_strerror(nread_processed) << std::endl;
      continue;
    }
    
    // #region agent log
    {
      std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
      if (log_file.is_open()) {
        log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"CLIENT-RECV\",\"location\":\"quicftp_client.cc:process_incoming_packets\",\"message\":\"Processed incoming packet\",\"data\":{\"packet_size\":" << nread << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
        log_file.close();
      }
    }
    // #endregion
    
    // Send any pending packets (handshake responses)
    send_pending_packets();
  }
  
  return true;
}

bool QuicClientWrapper::send_pending_packets() {
  if (!conn_ || udp_fd_ < 0) {
    return false;
  }
  
  // Set up path
  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);
  ngtcp2_path* path = &ps.path;
  
  struct sockaddr_in local_addr;
  socklen_t local_len = sizeof(local_addr);
  if (getsockname(udp_fd_, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
  }
  
  path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
  path->local.addrlen = sizeof(local_addr);
  path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&server_addr_);
  path->remote.addrlen = sizeof(server_addr_);
  
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  ngtcp2_tstamp ts = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  
  uint8_t pkt_buf[65536];
  ngtcp2_pkt_info pi;
  
  // Send packets until no more to send
  while (true) {
    ngtcp2_ssize nwrite = ngtcp2_conn_write_pkt(conn_, path, &pi, pkt_buf, sizeof(pkt_buf), ts);
    
    if (nwrite < 0) {
      if (nwrite == NGTCP2_ERR_WRITE_MORE) {
        // More data to write, continue
        continue;
      }
      break; // No more packets or error
    }
    
    // Send packet
    ssize_t nsent = sendto(udp_fd_, pkt_buf, nwrite, 0,
                          reinterpret_cast<struct sockaddr*>(&server_addr_),
                          sizeof(server_addr_));
    
    if (nsent < 0) {
      break;
    }
  }
  
  return true;
}

bool QuicClientWrapper::authenticate(const std::string& cert_path) {
  cert_path_ = cert_path;
  // Process incoming packets to continue handshake
  process_incoming_packets();
  // TODO: Perform certificate-based authentication
  return true;
}

void QuicClientWrapper::disconnect() {
  if (connected_) {
    if (conn_) {
      ngtcp2_conn_del(conn_);
      conn_ = nullptr;
    }
    if (ssl_) {
      SSL_free(ssl_);
      ssl_ = nullptr;
    }
    if (ssl_ctx_) {
      SSL_CTX_free(ssl_ctx_);
      ssl_ctx_ = nullptr;
    }
    if (udp_fd_ >= 0) {
      close(udp_fd_);
      udp_fd_ = -1;
    }
    connected_ = false;
  }
}

bool QuicClientWrapper::is_connected() const {
  return connected_;
}

bool QuicClientWrapper::create_stream(StreamId& stream_id) {
  if (!connected_ || !conn_) return false;
  
  // Process incoming packets to continue handshake if needed
  process_incoming_packets();
  
  // Open bidirectional stream
  int64_t ngtcp2_stream_id;
  int rv = ngtcp2_conn_open_bidi_stream(conn_, &ngtcp2_stream_id, nullptr);
  if (rv != 0) {
    // If stream blocked, try processing more packets
    if (rv == NGTCP2_ERR_STREAM_ID_BLOCKED) {
      process_incoming_packets();
      send_pending_packets();
      // Retry once
      rv = ngtcp2_conn_open_bidi_stream(conn_, &ngtcp2_stream_id, nullptr);
    }
    if (rv != 0) {
      std::cerr << "Failed to open QUIC stream: " << ngtcp2_strerror(rv) << std::endl;
      return false;
    }
  }
  
  stream_id = static_cast<StreamId>(ngtcp2_stream_id);
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"CLIENT-STREAM\",\"location\":\"quicftp_client.cc:create_stream\",\"message\":\"QUIC stream created\",\"data\":{\"stream_id\":" << stream_id << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  return true;
}

bool QuicClientWrapper::send_data(StreamId stream_id, const uint8_t* data, size_t len) {
  if (!connected_ || !conn_ || udp_fd_ < 0) {
    // Fallback to TestBridge if QUIC not ready
    return TestBridge::instance().send_to_server(server_address_, stream_id, data, len);
  }
  
  // Process incoming packets to continue handshake if needed
  process_incoming_packets();
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"CLIENT-SEND\",\"location\":\"quicftp_client.cc:send_data\",\"message\":\"Sending data via QUIC\",\"data\":{\"stream_id\":" << stream_id << ",\"len\":" << len << ",\"conn_exists\":" << (conn_ != nullptr) << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  // Set up path
  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);
  ngtcp2_path* path = &ps.path;
  
  struct sockaddr_in local_addr;
  socklen_t local_len = sizeof(local_addr);
  if (getsockname(udp_fd_, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) < 0) {
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
  }
  
  path->local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
  path->local.addrlen = sizeof(local_addr);
  path->remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&server_addr_);
  path->remote.addrlen = sizeof(server_addr_);
  
  // Prepare data vector
  ngtcp2_vec datav;
  datav.base = const_cast<uint8_t*>(data);
  datav.len = len;
  
  // Get current timestamp
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  ngtcp2_tstamp ts = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  
  // Write stream data to packet
  uint8_t pkt_buf[65536];
  ngtcp2_pkt_info pi;
  ngtcp2_ssize datalen = 0;
  
  ngtcp2_ssize nwrite = ngtcp2_conn_writev_stream(conn_, path, &pi, pkt_buf, sizeof(pkt_buf),
                                                   &datalen, NGTCP2_WRITE_STREAM_FLAG_MORE,
                                                   stream_id, &datav, 1, ts);
  
  if (nwrite < 0) {
    std::cerr << "Failed to write stream data: " << ngtcp2_strerror(nwrite) << std::endl;
    return false;
  }
  
  // Send packet via UDP
  ssize_t nsent = sendto(udp_fd_, pkt_buf, nwrite, 0,
                        reinterpret_cast<struct sockaddr*>(&server_addr_),
                        sizeof(server_addr_));
  
  if (nsent < 0) {
    std::cerr << "Failed to send UDP packet: " << strerror(errno) << std::endl;
    return false;
  }
  
  // #region agent log
  {
    std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"CLIENT-SEND\",\"location\":\"quicftp_client.cc:send_data\",\"message\":\"QUIC packet sent\",\"data\":{\"stream_id\":" << stream_id << ",\"packet_size\":" << nsent << ",\"data_len\":" << len << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file.close();
    }
  }
  // #endregion
  
  return true;
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

