// stream_manager.h
// Stream management for concurrent file transfers

#ifndef STREAM_MANAGER_H
#define STREAM_MANAGER_H

#include "quic_common.h"
#include <map>
#include <mutex>
#include <string>
#include <chrono>

namespace quicftp {

// Stream information
struct StreamInfo {
  StreamId id;
  StreamState state;
  std::string file_path;
  size_t bytes_transferred;
  size_t total_bytes;
  std::chrono::steady_clock::time_point start_time;
  double current_speed; // bytes per second
  double average_speed; // bytes per second
  int priority; // Higher = more priority
  bool is_upload; // true for upload, false for download
};

// Stream manager for tracking multiple concurrent streams
class StreamManager {
public:
  StreamManager();
  ~StreamManager();

  // Stream lifecycle
  StreamId create_stream(const std::string& file_path, size_t total_bytes, int priority, bool is_upload);
  bool update_stream(StreamId id, size_t bytes_transferred);
  void complete_stream(StreamId id);
  void error_stream(StreamId id, const std::string& error);
  void remove_stream(StreamId id);

  // Stream queries
  StreamInfo* get_stream(StreamId id);
  std::vector<StreamId> get_active_streams() const;
  std::vector<StreamId> get_streams_by_priority(int priority) const;
  size_t get_total_active_bytes() const;

  // Statistics
  size_t get_total_bytes_transferred() const;
  double get_total_average_speed() const;

private:
  mutable std::mutex mutex_;
  std::map<StreamId, StreamInfo> streams_;
  StreamId next_stream_id_;
  
  void update_speed(StreamInfo& info);
};

} // namespace quicftp

#endif

