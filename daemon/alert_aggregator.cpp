#include "alert_aggregator.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace faw {
namespace phm {

class AlertAggregator::Impl {
 public:
  Impl()
      : suppression_duration_(std::chrono::minutes(5)),
        escalation_duration_(std::chrono::minutes(30)) {}

  void addEvent(const PhmEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = event.source_se + "/" + event.source_monitor + "/" +
                      std::to_string(static_cast<int>(event.severity));

    auto now = std::chrono::system_clock::now();

    // 检查抑制
    auto it = last_suppressed_.find(key);
    if (it != last_suppressed_.end()) {
      if (now - it->second < suppression_duration_) {
        return;  // 在抑制期内
      }
    }

    events_.push_back(event);
    last_suppressed_[key] = now;
  }

  std::vector<PhmEvent> getAggregatedAlerts() {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
  }

  void setSuppressionDuration(std::chrono::minutes duration) {
    suppression_duration_ = duration;
  }

  void setEscalationDuration(std::chrono::minutes duration) {
    escalation_duration_ = duration;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    last_suppressed_.clear();
  }

 private:
  std::vector<PhmEvent> events_;
  std::map<std::string, std::chrono::system_clock::time_point> last_suppressed_;
  std::chrono::minutes suppression_duration_;
  std::chrono::minutes escalation_duration_;
  std::mutex mutex_;
};

// =============================================================================
// AlertAggregator 公共接口
// =============================================================================

AlertAggregator::AlertAggregator() : impl_(std::make_unique<Impl>()) {}

AlertAggregator::~AlertAggregator() = default;

void AlertAggregator::addEvent(const PhmEvent& event) {
  impl_->addEvent(event);
}

std::vector<PhmEvent> AlertAggregator::getAggregatedAlerts() {
  return impl_->getAggregatedAlerts();
}

void AlertAggregator::setSuppressionDuration(std::chrono::minutes duration) {
  impl_->setSuppressionDuration(duration);
}

void AlertAggregator::setEscalationDuration(std::chrono::minutes duration) {
  impl_->setEscalationDuration(duration);
}

void AlertAggregator::clear() { impl_->clear(); }

}  // namespace phm
}  // namespace faw