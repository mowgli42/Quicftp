// stream_manager.cc

#include "stream_manager.h"
#include <algorithm>
#include <numeric>

namespace quicftp {

StreamManager::StreamManager() : next_stream_id_(1) {
}

StreamManager::~StreamManager() = default;

StreamId StreamManager::create_stream(const std::string& file_path, size_t total_bytes, int priority, bool is_upload) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  StreamId id = next_stream_id_++;
  StreamInfo info;
  info.id = id;
  info.state = StreamState::Open;
  info.file_path = file_path;
  info.bytes_transferred = 0;
  info.total_bytes = total_bytes;
  info.start_time = std::chrono::steady_clock::now();
  info.current_speed = 0.0;
  info.average_speed = 0.0;
  info.priority = priority;
  info.is_upload = is_upload;
  
  streams_[id] = info;
  return id;
}

bool StreamManager::update_stream(StreamId id, size_t bytes_transferred) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = streams_.find(id);
  if (it == streams_.end()) {
    return false;
  }
  
  it->second.bytes_transferred = bytes_transferred;
  it->second.state = StreamState::Open;
  update_speed(it->second);
  return true;
}

void StreamManager::complete_stream(StreamId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = streams_.find(id);
  if (it != streams_.end()) {
    it->second.state = StreamState::Closed;
    update_speed(it->second);
  }
}

void StreamManager::error_stream(StreamId id, const std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = streams_.find(id);
  if (it != streams_.end()) {
    it->second.state = StreamState::Error;
  }
}

void StreamManager::remove_stream(StreamId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  streams_.erase(id);
}

StreamInfo* StreamManager::get_stream(StreamId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = streams_.find(id);
  return (it != streams_.end()) ? &it->second : nullptr;
}

std::vector<StreamId> StreamManager::get_active_streams() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<StreamId> active;
  
  for (const auto& [id, info] : streams_) {
    if (info.state == StreamState::Open) {
      active.push_back(id);
    }
  }
  
  return active;
}

std::vector<StreamId> StreamManager::get_streams_by_priority(int priority) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<StreamId> result;
  
  for (const auto& [id, info] : streams_) {
    if (info.priority == priority && info.state == StreamState::Open) {
      result.push_back(id);
    }
  }
  
  return result;
}

size_t StreamManager::get_total_active_bytes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  size_t total = 0;
  for (const auto& [id, info] : streams_) {
    if (info.state == StreamState::Open) {
      total += info.bytes_transferred;
    }
  }
  
  return total;
}

size_t StreamManager::get_total_bytes_transferred() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  size_t total = 0;
  for (const auto& [id, info] : streams_) {
    total += info.bytes_transferred;
  }
  
  return total;
}

double StreamManager::get_total_average_speed() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (streams_.empty()) return 0.0;
  
  double total_speed = 0.0;
  size_t count = 0;
  
  for (const auto& [id, info] : streams_) {
    if (info.average_speed > 0.0) {
      total_speed += info.average_speed;
      count++;
    }
  }
  
  return (count > 0) ? total_speed / count : 0.0;
}

void StreamManager::update_speed(StreamInfo& info) {
  auto now = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.start_time).count();
  
  if (duration > 0) {
    info.average_speed = (static_cast<double>(info.bytes_transferred) / duration) * 1000.0;
    
    // For current speed, use last update interval (simplified - would need more tracking for accurate current speed)
    info.current_speed = info.average_speed;
  }
}

} // namespace quicftp

