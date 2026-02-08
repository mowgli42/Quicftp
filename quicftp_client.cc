// quicftp_client.cc

#include "quicftp_client.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>

namespace quicftp {

// ============================================================================
// Error handling implementation
// ============================================================================

const char* error_code_str(ErrorCode code) {
  switch (code) {
    case ErrorCode::Ok:                   return "Ok";
    case ErrorCode::ConnectionFailed:     return "ConnectionFailed";
    case ErrorCode::AuthenticationFailed: return "AuthenticationFailed";
    case ErrorCode::FileNotFound:         return "FileNotFound";
    case ErrorCode::FileOpenError:        return "FileOpenError";
    case ErrorCode::UploadFailed:         return "UploadFailed";
    case ErrorCode::DownloadFailed:       return "DownloadFailed";
    case ErrorCode::ServerError:          return "ServerError";
    case ErrorCode::Timeout:              return "Timeout";
    case ErrorCode::SSLError:             return "SSLError";
    case ErrorCode::NotConnected:         return "NotConnected";
    case ErrorCode::InvalidArgument:      return "InvalidArgument";
    case ErrorCode::Unknown:              return "Unknown";
  }
  return "Unknown";
}

TransferResult TransferResult::ok(size_t bytes) {
  TransferResult r;
  r.success = true;
  r.error = ErrorCode::Ok;
  r.bytes_transferred = bytes;
  return r;
}

TransferResult TransferResult::fail(ErrorCode code, const std::string& msg, long http_status) {
  TransferResult r;
  r.success = false;
  r.error = code;
  r.message = msg;
  r.http_status = http_status;
  return r;
}

// Map CURLcode to our ErrorCode
static ErrorCode curl_to_error_code(CURLcode code) {
  switch (code) {
    case CURLE_OK:                  return ErrorCode::Ok;
    case CURLE_COULDNT_CONNECT:     return ErrorCode::ConnectionFailed;
    case CURLE_COULDNT_RESOLVE_HOST:return ErrorCode::ConnectionFailed;
    case CURLE_OPERATION_TIMEDOUT:  return ErrorCode::Timeout;
    case CURLE_SSL_CONNECT_ERROR:   return ErrorCode::SSLError;
    case CURLE_SSL_CERTPROBLEM:     return ErrorCode::SSLError;
    case CURLE_SSL_CACERT:          return ErrorCode::SSLError;
    case CURLE_REMOTE_FILE_NOT_FOUND: return ErrorCode::FileNotFound;
    default:                        return ErrorCode::Unknown;
  }
}

// Map HTTP status code to our ErrorCode
static ErrorCode http_status_to_error_code(long status) {
  if (status >= 200 && status < 300) return ErrorCode::Ok;
  if (status == 401 || status == 403) return ErrorCode::AuthenticationFailed;
  if (status == 404) return ErrorCode::FileNotFound;
  if (status == 408) return ErrorCode::Timeout;
  if (status >= 500) return ErrorCode::ServerError;
  return ErrorCode::Unknown;
}

// ============================================================================
// Helper callbacks
// ============================================================================

namespace {

// Helper to write callback for downloading
size_t write_data_cb(void* ptr, size_t size, size_t nmemb, void* stream) {
  size_t written = 0;
  if (stream) {
    std::ofstream* out = static_cast<std::ofstream*>(stream);
    out->write(static_cast<char*>(ptr), size * nmemb);
    written = size * nmemb;
  }
  return written;
}

// Helper to read callback for uploading
size_t read_data_cb(char* ptr, size_t size, size_t nmemb, void* stream) {
  size_t retcode = 0;
  if (stream) {
    std::ifstream* in = static_cast<std::ifstream*>(stream);
    in->read(ptr, size * nmemb);
    retcode = in->gcount();
  }
  return retcode;
}

// Progress callback bridge
struct ProgressData {
  std::string file_path;
  ProgressCallback* callback;
};

int xfer_info_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                 curl_off_t ultotal, curl_off_t ulnow) {
  auto* pd = static_cast<ProgressData*>(clientp);
  if (pd && pd->callback && *pd->callback) {
    size_t total = static_cast<size_t>(dltotal > 0 ? dltotal : ultotal);
    size_t now = static_cast<size_t>(dlnow > 0 ? dlnow : ulnow);
    (*pd->callback)(pd->file_path, now, total);
  }
  return 0; // 0 = continue, non-zero = abort
}

// Debug callback for curl verbose output
int debug_cb(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    (void)handle;
    (void)userptr;
    if(type == CURLINFO_TEXT) {
        std::cerr << "CURL: " << std::string(data, size);
    }
    return 0;
}

} // namespace

// ============================================================================
// Client::Impl
// ============================================================================

class Client::Impl {
public:
  std::string server_url_;
  std::string cert_path_;
  std::string key_path_;
  std::string ca_cert_path_;
  bool verbose_ = false;
  bool insecure_ = true; // Default: insecure for backward compat (Phase 2 TODO: flip default)
  size_t upload_bps_ = 0;
  size_t download_bps_ = 0;
  ProgressCallback progress_callback_;
  std::mutex mutex_;

  Impl() {
    curl_global_init(CURL_GLOBAL_ALL);
  }

  ~Impl() {
    curl_global_cleanup();
  }

  void configure_handle(CURL* curl, ProgressData* pd = nullptr) {
      if (verbose_) {
          curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
          curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_cb);
      }

      // Force HTTP/3 (falls back to HTTP/2 if not supported)
      curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
      
      // TLS Configuration
      if (insecure_) {
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      } else {
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
          if (!ca_cert_path_.empty()) {
              curl_easy_setopt(curl, CURLOPT_CAINFO, ca_cert_path_.c_str());
          }
      }
      
      if (!cert_path_.empty()) {
          curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path_.c_str());
      }
      if (!key_path_.empty()) {
          curl_easy_setopt(curl, CURLOPT_SSLKEY, key_path_.c_str());
      }

      // Bandwidth limiting
      if (upload_bps_ > 0) {
          curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, static_cast<curl_off_t>(upload_bps_));
      }
      if (download_bps_ > 0) {
          curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, static_cast<curl_off_t>(download_bps_));
      }

      // Progress callback
      if (pd && progress_callback_) {
          curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_info_cb);
          curl_easy_setopt(curl, CURLOPT_XFERINFODATA, pd);
          curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
      }
  }
};

// ============================================================================
// Client public API
// ============================================================================

Client::Client() : impl_(std::make_unique<Impl>()) {}

Client::~Client() = default;

bool Client::connect(const std::string& server_url) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    // Normalize URL (ensure no trailing slash)
    std::string url = server_url;
    if (!url.empty() && url.back() == '/') url.pop_back();
    
    // Ensure scheme is present (default to https)
    if (url.find("://") == std::string::npos) {
        url = "https://" + url;
    }

    impl_->server_url_ = url;
    return true;
}

bool Client::authenticate(const std::string& cert_path, const std::string& key_path) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (!std::filesystem::exists(cert_path)) {
        std::cerr << "Certificate not found: " << cert_path << std::endl;
        return false;
    }
    impl_->cert_path_ = cert_path;
    impl_->key_path_ = key_path;
    return true;
}

void Client::set_verbose(bool verbose) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->verbose_ = verbose;
}

void Client::set_progress_callback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->progress_callback_ = callback;
}

void Client::set_ca_cert(const std::string& ca_path) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->ca_cert_path_ = ca_path;
    if (!ca_path.empty()) {
        impl_->insecure_ = false; // Auto-enable verification when CA cert is set
    }
}

void Client::set_insecure(bool insecure) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->insecure_ = insecure;
}

void Client::set_bandwidth_limit(size_t upload_bps, size_t download_bps) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->upload_bps_ = upload_bps;
    impl_->download_bps_ = download_bps;
}

// ============================================================================
// Structured transfer methods
// ============================================================================

TransferResult Client::upload(const std::string& local_path, const std::string& remote_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return TransferResult::fail(ErrorCode::Unknown, "Failed to initialize curl");

    std::string full_url;
    ProgressData pd{local_path, nullptr};
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        if (impl_->server_url_.empty()) {
            curl_easy_cleanup(curl);
            return TransferResult::fail(ErrorCode::NotConnected, "Not connected. Call connect() first.");
        }
        full_url = impl_->server_url_ + "/files/" + remote_path;
        pd.callback = &impl_->progress_callback_;
        impl_->configure_handle(curl, &pd);
    }

    // Open file
    std::ifstream file(local_path, std::ios::binary);
    if (!file) {
        curl_easy_cleanup(curl);
        return TransferResult::fail(ErrorCode::FileOpenError, 
            "Could not open local file: " + local_path);
    }

    // Get file size
    file.seekg(0, std::ios::end);
    curl_off_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_data_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA, &file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, file_size);

    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    TransferResult result;
    if (res != CURLE_OK) {
        result = TransferResult::fail(curl_to_error_code(res),
            std::string("Upload failed: ") + curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        result.http_status = http_code;
        
        if (http_code >= 200 && http_code < 300) {
            result.success = true;
            result.error = ErrorCode::Ok;
            result.bytes_transferred = static_cast<size_t>(file_size);
        } else {
            result = TransferResult::fail(http_status_to_error_code(http_code),
                "Upload failed with HTTP " + std::to_string(http_code), http_code);
        }
    }

    curl_easy_cleanup(curl);
    return result;
}

TransferResult Client::download(const std::string& remote_path, const std::string& local_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return TransferResult::fail(ErrorCode::Unknown, "Failed to initialize curl");

    std::string full_url;
    ProgressData pd{remote_path, nullptr};
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        if (impl_->server_url_.empty()) {
            curl_easy_cleanup(curl);
            return TransferResult::fail(ErrorCode::NotConnected, "Not connected. Call connect() first.");
        }
        full_url = impl_->server_url_ + "/files/" + remote_path;
        pd.callback = &impl_->progress_callback_;
        impl_->configure_handle(curl, &pd);
    }

    // Open output file
    std::ofstream file(local_path, std::ios::binary);
    if (!file) {
        curl_easy_cleanup(curl);
        return TransferResult::fail(ErrorCode::FileOpenError,
            "Could not open local file for writing: " + local_path);
    }

    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    TransferResult result;
    if (res != CURLE_OK) {
        result = TransferResult::fail(curl_to_error_code(res),
            std::string("Download failed: ") + curl_easy_strerror(res));
        file.close();
        // Remove partial file on curl error
        std::filesystem::remove(local_path);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        result.http_status = http_code;

        // Get bytes downloaded
        curl_off_t dl_size = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl_size);

        if (http_code >= 200 && http_code < 300) {
            result.success = true;
            result.error = ErrorCode::Ok;
            result.bytes_transferred = static_cast<size_t>(dl_size);
        } else {
            result = TransferResult::fail(http_status_to_error_code(http_code),
                "Download failed with HTTP " + std::to_string(http_code), http_code);
            file.close();
            // Remove file that contains error body (e.g., "Not Found")
            std::filesystem::remove(local_path);
        }
    }

    curl_easy_cleanup(curl);
    return result;
}

// ============================================================================
// Batch transfer context
// ============================================================================

struct TransferContext {
    CURL* curl;
    std::string local_path;
    std::string remote_path;
    std::unique_ptr<std::fstream> stream;
    ProgressData progress_data;
};

BatchTransferResult Client::upload_batch(const std::vector<std::pair<std::string, std::string>>& files) {
    BatchTransferResult batch;
    if (files.empty()) {
        batch.all_success = true;
        return batch;
    }

    CURLM* multi_handle = curl_multi_init();
    std::vector<std::unique_ptr<TransferContext>> transfers;
    int still_running = 0;

    // Initialize all transfers
    for (const auto& [local, remote] : files) {
        auto ctx = std::make_unique<TransferContext>();
        ctx->local_path = local;
        ctx->remote_path = remote;
        ctx->curl = curl_easy_init();
        ctx->progress_data = {local, nullptr};
        
        // Open file
        ctx->stream = std::make_unique<std::fstream>(local, std::ios::binary | std::ios::in);
        if (!ctx->stream->is_open()) {
            batch.results.push_back({local, 
                TransferResult::fail(ErrorCode::FileOpenError, "Failed to open: " + local)});
            batch.failed_count++;
            curl_easy_cleanup(ctx->curl);
            continue; 
        }

        // Get file size
        ctx->stream->seekg(0, std::ios::end);
        curl_off_t file_size = ctx->stream->tellg();
        ctx->stream->seekg(0, std::ios::beg);

        // Configure
        {
            std::lock_guard<std::mutex> lock(impl_->mutex_);
            ctx->progress_data.callback = &impl_->progress_callback_;
            impl_->configure_handle(ctx->curl, &ctx->progress_data);
            std::string full_url = impl_->server_url_ + "/files/" + remote;
            curl_easy_setopt(ctx->curl, CURLOPT_URL, full_url.c_str());
        }
        
        curl_easy_setopt(ctx->curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(ctx->curl, CURLOPT_READFUNCTION, read_data_cb);
        curl_easy_setopt(ctx->curl, CURLOPT_READDATA, ctx->stream.get());
        curl_easy_setopt(ctx->curl, CURLOPT_INFILESIZE_LARGE, file_size);
        curl_easy_setopt(ctx->curl, CURLOPT_PRIVATE, ctx.get());

        curl_multi_add_handle(multi_handle, ctx->curl);
        transfers.push_back(std::move(ctx));
    }

    // Event Loop
    curl_multi_perform(multi_handle, &still_running);
    while (still_running) {
        int numfds;
        curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
        curl_multi_perform(multi_handle, &still_running);
    }

    // Check results
    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            TransferContext* ctx = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
            
            TransferResult result;
            if (msg->data.result != CURLE_OK) {
                result = TransferResult::fail(curl_to_error_code(msg->data.result),
                    "Upload failed for " + ctx->local_path + ": " + curl_easy_strerror(msg->data.result));
                batch.failed_count++;
            } else {
                long http_code = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
                result.http_status = http_code;

                if (http_code >= 200 && http_code < 300) {
                    result.success = true;
                    result.error = ErrorCode::Ok;
                    batch.succeeded_count++;
                    if (impl_->verbose_) std::cout << "Uploaded " << ctx->local_path << std::endl;
                } else {
                    result = TransferResult::fail(http_status_to_error_code(http_code),
                        "Upload failed with HTTP " + std::to_string(http_code) + " for " + ctx->local_path, http_code);
                    batch.failed_count++;
                }
            }
            batch.results.push_back({ctx->local_path, result});
            curl_multi_remove_handle(multi_handle, msg->easy_handle);
        }
    }

    // Cleanup
    for (auto& ctx : transfers) {
        curl_easy_cleanup(ctx->curl);
    }
    curl_multi_cleanup(multi_handle);
    
    batch.all_success = (batch.failed_count == 0);
    return batch;
}

BatchTransferResult Client::download_batch(const std::vector<std::pair<std::string, std::string>>& files) {
    BatchTransferResult batch;
    if (files.empty()) {
        batch.all_success = true;
        return batch;
    }

    CURLM* multi_handle = curl_multi_init();
    std::vector<std::unique_ptr<TransferContext>> transfers;
    int still_running = 0;

    // Initialize all transfers
    for (const auto& [remote, local] : files) {
        auto ctx = std::make_unique<TransferContext>();
        ctx->local_path = local;
        ctx->remote_path = remote;
        ctx->curl = curl_easy_init();
        ctx->progress_data = {remote, nullptr};
        
        // Open file for writing
        ctx->stream = std::make_unique<std::fstream>(local, std::ios::binary | std::ios::out);
        if (!ctx->stream->is_open()) {
            batch.results.push_back({remote, 
                TransferResult::fail(ErrorCode::FileOpenError, "Failed to open for writing: " + local)});
            batch.failed_count++;
            curl_easy_cleanup(ctx->curl);
            continue; 
        }

        // Configure
        {
            std::lock_guard<std::mutex> lock(impl_->mutex_);
            ctx->progress_data.callback = &impl_->progress_callback_;
            impl_->configure_handle(ctx->curl, &ctx->progress_data);
            std::string full_url = impl_->server_url_ + "/files/" + remote;
            curl_easy_setopt(ctx->curl, CURLOPT_URL, full_url.c_str());
        }

        curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, write_data_cb);
        curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->stream.get());
        curl_easy_setopt(ctx->curl, CURLOPT_PRIVATE, ctx.get());

        curl_multi_add_handle(multi_handle, ctx->curl);
        transfers.push_back(std::move(ctx));
    }

    // Event Loop
    curl_multi_perform(multi_handle, &still_running);
    while (still_running) {
        int numfds;
        curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
        curl_multi_perform(multi_handle, &still_running);
    }

    // Check results
    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            TransferContext* ctx = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
            
            TransferResult result;
            if (msg->data.result != CURLE_OK) {
                result = TransferResult::fail(curl_to_error_code(msg->data.result),
                    "Download failed for " + ctx->remote_path + ": " + curl_easy_strerror(msg->data.result));
                batch.failed_count++;
                ctx->stream->close();
                std::filesystem::remove(ctx->local_path);
            } else {
                long http_code = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
                result.http_status = http_code;

                curl_off_t dl_size = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_SIZE_DOWNLOAD_T, &dl_size);

                if (http_code >= 200 && http_code < 300) {
                    result.success = true;
                    result.error = ErrorCode::Ok;
                    result.bytes_transferred = static_cast<size_t>(dl_size);
                    batch.succeeded_count++;
                    if (impl_->verbose_) std::cout << "Downloaded " << ctx->remote_path << std::endl;
                } else {
                    result = TransferResult::fail(http_status_to_error_code(http_code),
                        "Download failed with HTTP " + std::to_string(http_code) + " for " + ctx->remote_path, http_code);
                    batch.failed_count++;
                    ctx->stream->close();
                    std::filesystem::remove(ctx->local_path);
                }
            }
            batch.results.push_back({ctx->remote_path, result});
            curl_multi_remove_handle(multi_handle, msg->easy_handle);
        }
    }

    // Cleanup
    for (auto& ctx : transfers) {
        curl_easy_cleanup(ctx->curl);
    }
    curl_multi_cleanup(multi_handle);
    
    batch.all_success = (batch.failed_count == 0);
    return batch;
}

// ============================================================================
// Legacy bool API (delegates to structured API)
// ============================================================================

bool Client::upload_file(const std::string& local_path, const std::string& remote_path) {
    auto result = upload(local_path, remote_path);
    if (!result.success) {
        std::cerr << "Upload failed: " << result.message << std::endl;
    }
    return result.success;
}

bool Client::download_file(const std::string& remote_path, const std::string& local_path) {
    auto result = download(remote_path, local_path);
    if (!result.success) {
        std::cerr << "Download failed: " << result.message << std::endl;
    }
    return result.success;
}

bool Client::upload_files(const std::vector<std::pair<std::string, std::string>>& files) {
    auto result = upload_batch(files);
    return result.all_success;
}

bool Client::download_files(const std::vector<std::pair<std::string, std::string>>& files) {
    auto result = download_batch(files);
    return result.all_success;
}

} // namespace quicftp
