#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "faw/phm/logger.h"
#include "faw/phm/types.h"

namespace faw {
namespace phm {

/// 事件管理器（内部实现）
///
/// 管理有界环形缓冲区的事件队列。
/// 线程安全。
class EventManager {
 public:
  explicit EventManager(size_t max_size = 1024) : max_size_(max_size) {}

  /// 添加事件
  bool push(const PhmEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.size() >= max_size_) {
      // 队列满，丢弃最旧事件
      queue_.pop_front();
      PHM_LOG_WARN("Event queue full, dropping oldest event");
    }

    queue_.push_back(event);
    return true;
  }

  /// 获取所有事件（按最低严重级别过滤）
  std::vector<PhmEvent> getEvents(
      Severity min_severity = Severity::INFO) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PhmEvent> result;
    for (const auto& evt : queue_) {
      if (static_cast<uint8_t>(evt.severity) >=
          static_cast<uint8_t>(min_severity)) {
        result.push_back(evt);
      }
    }
    return result;
  }

  /// 确认事件（移除指定 ID 的事件）
  size_t acknowledge(const std::vector<std::string>& event_ids) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    auto it = queue_.begin();
    while (it != queue_.end()) {
      if (std::find(event_ids.begin(), event_ids.end(), it->id) !=
          event_ids.end()) {
        it = queue_.erase(it);
        count++;
      } else {
        ++it;
      }
    }
    return count;
  }

  /// 清空事件队列
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
  }

  /// 获取待处理事件数
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  /// 获取队列容量
  size_t capacity() const noexcept { return max_size_; }

  /// 清扫过期事件（超过 TTL 的自动移除）
  size_t purge(std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto cutoff = std::chrono::system_clock::now() - ttl;
    size_t count = 0;

    auto it = queue_.begin();
    while (it != queue_.end()) {
      if (it->timestamp < cutoff) {
        it = queue_.erase(it);
        count++;
      } else {
        ++it;
      }
    }
    return count;
  }

 private:
  size_t max_size_;
  std::deque<PhmEvent> queue_;
  mutable std::mutex mutex_;
};

}  // namespace phm
}  // namespace faw