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

// Debug callback for curl verbose output
int debug_cb(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    (void)handle;
    (void)userptr;
    if(type == CURLINFO_TEXT) {
        // std::cerr << "CURL: " << std::string(data, size);
    } else if (type == CURLINFO_HEADER_OUT) {
        // std::cerr << "SEND HEADER: " << std::string(data, size);
    } else if (type == CURLINFO_HEADER_IN) {
        // std::cerr << "RECV HEADER: " << std::string(data, size);
    }
    return 0;
}

} // namespace

class Client::Impl {
public:
  std::string server_url_;
  std::string cert_path_;
  std::string key_path_;
  bool verbose_ = false;
  ProgressCallback progress_callback_;
  std::mutex mutex_;

  Impl() {
    curl_global_init(CURL_GLOBAL_ALL);
  }

  ~Impl() {
    curl_global_cleanup();
  }

  void configure_handle(CURL* curl) {
      if (verbose_) {
          curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
          curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_cb);
      }

      // Force HTTP/3
      curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
      
      // TLS Configuration
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // For self-signed certs in dev
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      
      if (!cert_path_.empty()) {
          curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path_.c_str());
      }
      if (!key_path_.empty()) {
          curl_easy_setopt(curl, CURLOPT_SSLKEY, key_path_.c_str());
      }
  }
};

Client::Client() : impl_(std::make_unique<Impl>()) {}

Client::~Client() = default;

bool Client::connect(const std::string& server_url) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    // Normalize URL (ensure no trailing slash)
    std::string url = server_url;
    if (url.back() == '/') url.pop_back();
    
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

bool Client::upload_file(const std::string& local_path, const std::string& remote_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string full_url;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        if (impl_->server_url_.empty()) {
            std::cerr << "Error: Not connected. Call connect() first." << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }
        full_url = impl_->server_url_ + "/files/" + remote_path;
        impl_->configure_handle(curl);
    }

    // Open file
    std::ifstream file(local_path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open local file " << local_path << std::endl;
        curl_easy_cleanup(curl);
        return false;
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
    
    if (res != CURLE_OK) {
        std::cerr << "Upload failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

bool Client::download_file(const std::string& remote_path, const std::string& local_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string full_url;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        if (impl_->server_url_.empty()) {
            std::cerr << "Error: Not connected. Call connect() first." << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }
        full_url = impl_->server_url_ + "/files/" + remote_path;
        impl_->configure_handle(curl);
    }

    // Open output file
    std::ofstream file(local_path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open local file for writing " << local_path << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

struct TransferContext {
    CURL* curl;
    std::string local_path;
    std::string remote_path;
    std::unique_ptr<std::fstream> stream; // fstream for both read/write
};

bool Client::upload_files(const std::vector<std::pair<std::string, std::string>>& files) {
    if (files.empty()) return true;

    CURLM* multi_handle = curl_multi_init();
    std::vector<std::unique_ptr<TransferContext>> transfers;
    int still_running = 0;

    // Initialize all transfers
    for (const auto& [local, remote] : files) {
        auto ctx = std::make_unique<TransferContext>();
        ctx->local_path = local;
        ctx->remote_path = remote;
        ctx->curl = curl_easy_init();
        
        // Open file
        ctx->stream = std::make_unique<std::fstream>(local, std::ios::binary | std::ios::in);
        if (!ctx->stream->is_open()) {
            std::cerr << "Failed to open " << local << std::endl;
            curl_easy_cleanup(ctx->curl);
            continue; 
        }

        // Get file size
        ctx->stream->seekg(0, std::ios::end);
        curl_off_t file_size = ctx->stream->tellg();
        ctx->stream->seekg(0, std::ios::beg);

        // Configure
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        impl_->configure_handle(ctx->curl);
        std::string full_url = impl_->server_url_ + "/files/" + remote;
        
        curl_easy_setopt(ctx->curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(ctx->curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(ctx->curl, CURLOPT_READFUNCTION, read_data_cb);
        curl_easy_setopt(ctx->curl, CURLOPT_READDATA, ctx->stream.get()); // Use get() to pass raw pointer
        curl_easy_setopt(ctx->curl, CURLOPT_INFILESIZE_LARGE, file_size);
        curl_easy_setopt(ctx->curl, CURLOPT_PRIVATE, ctx.get()); // Store context pointer

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
    bool all_success = true;
    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            TransferContext* ctx = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
            
            if (msg->data.result != CURLE_OK) {
                std::cerr << "Parallel upload failed for " << ctx->local_path 
                          << ": " << curl_easy_strerror(msg->data.result) << std::endl;
                all_success = false;
            } else {
                if(impl_->verbose_) std::cout << "Uploaded " << ctx->local_path << std::endl;
            }
            curl_multi_remove_handle(multi_handle, msg->easy_handle);
        }
    }

    // Cleanup
    for(auto& ctx : transfers) {
        curl_easy_cleanup(ctx->curl);
    }
    curl_multi_cleanup(multi_handle);
    
    return all_success;
}

bool Client::download_files(const std::vector<std::pair<std::string, std::string>>& files) {
     if (files.empty()) return true;

    CURLM* multi_handle = curl_multi_init();
    std::vector<std::unique_ptr<TransferContext>> transfers;
    int still_running = 0;

    // Initialize all transfers
    for (const auto& [remote, local] : files) {
        auto ctx = std::make_unique<TransferContext>();
        ctx->local_path = local;
        ctx->remote_path = remote;
        ctx->curl = curl_easy_init();
        
        // Open file for writing
        ctx->stream = std::make_unique<std::fstream>(local, std::ios::binary | std::ios::out);
        if (!ctx->stream->is_open()) {
            std::cerr << "Failed to open for writing " << local << std::endl;
            curl_easy_cleanup(ctx->curl);
            continue; 
        }

        // Configure
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        impl_->configure_handle(ctx->curl);
        std::string full_url = impl_->server_url_ + "/files/" + remote;
        
        curl_easy_setopt(ctx->curl, CURLOPT_URL, full_url.c_str());
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
    bool all_success = true;
    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            TransferContext* ctx = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
            
            if (msg->data.result != CURLE_OK) {
                std::cerr << "Parallel download failed for " << ctx->remote_path 
                          << ": " << curl_easy_strerror(msg->data.result) << std::endl;
                all_success = false;
            } else {
                 if(impl_->verbose_) std::cout << "Downloaded " << ctx->remote_path << std::endl;
            }
            curl_multi_remove_handle(multi_handle, msg->easy_handle);
        }
    }

    // Cleanup
    for(auto& ctx : transfers) {
        curl_easy_cleanup(ctx->curl);
    }
    curl_multi_cleanup(multi_handle);
    
    return all_success;
}

} // namespace quicftp
