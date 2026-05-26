#include <atomic>
#include <chrono>
#include <string>

#include "faw/phm/health_channel.h"
#include "faw/phm/logger.h"
#include "faw/phm/monitor.h"

namespace faw {
namespace phm {

/// Alive 活性监控器
///
/// 通过 HealthChannel 检测被监督进程的心跳活性。
/// 被监督进程应定期递增 HealthChannel 的 AliveCounter，
/// AliveMonitor 检测该计数器是否按预期增长。
class AliveMonitor : public IMonitor {
 public:
  AliveMonitor(const std::string& name, const MonitorConfig& cfg)
      : name_(name),
        config_(cfg),
        state_(SeState::INIT),
        last_counter_(0),
        first_check_(true) {
    auto it = cfg.params.find("health_channel");
    if (it != cfg.params.end()) channel_name_ = it->second;
  }

  bool start() override {
    state_ = SeState::RUNNING;
    last_counter_ = 0;
    first_check_ = true;
    PHM_LOG_INFO("AliveMonitor '%s' started", name_.c_str());
    return true;
  }

  void stop() override { state_ = SeState::INIT; }

  void reset() override {
    state_ = SeState::INIT;
    last_counter_ = 0;
    first_check_ = true;
  }

  PhmEvent check() override {
    PhmEvent event;
    if (!health_channel_) return event;

    uint64_t current = health_channel_->getAliveCounter();
    bool alive = health_channel_->isAlive();

    if (first_check_) {
      // 首次检查，建立基线
      last_counter_ = current;
      first_check_ = false;
      return event;
    }

    if (!alive || current == last_counter_) {
      // 计数器冻结或超时 → 疑似挂死
      state_ = SeState::WARN;

      event.id = generateId();
      event.source_monitor = name_;
      event.severity = Severity::WARNING;
      event.state = SeState::WARN;
      event.message =
          "Alive heartbeat not detected (counter=" + std::to_string(current) +
          ", last=" + std::to_string(last_counter_) + ")";
      event.timestamp = std::chrono::system_clock::now();
      event.metadata["alive_counter"] = std::to_string(current);
      event.metadata["last_counter"] = std::to_string(last_counter_);
    } else if (current > last_counter_) {
      // 计数器正常递增
      if (state_ != SeState::RUNNING) {
        state_ = SeState::RUNNING;
      }
    }

    last_counter_ = current;
    return event;
  }

  SeState getState() const override { return state_; }
  MonitorType getType() const noexcept override { return MonitorType::ALIVE; }

  void configure(const MonitorConfig& cfg) override { config_ = cfg; }

  const MonitorConfig& getConfig() const noexcept override { return config_; }
  const std::string& name() const noexcept override { return name_; }

  void setEventCallback(EventCallback cb) override {
    callback_ = std::move(cb);
  }

  void setHealthChannel(HealthChannel* hc) { health_channel_ = hc; }

  std::map<std::string, double> collectMetrics() override {
    std::map<std::string, double> m;
    m["alive_counter"] = static_cast<double>(last_counter_);
    return m;
  }

 private:
  static std::string generateId() {
    static std::atomic<uint64_t> counter{0};
    return "phm_mon_alive_" + std::to_string(++counter) + "_" +
           std::to_string(
               std::chrono::system_clock::now().time_since_epoch().count());
  }

  std::string name_;
  MonitorConfig config_;
  std::string channel_name_;
  std::atomic<SeState> state_;
  uint64_t last_counter_;
  bool first_check_;
  HealthChannel* health_channel_{nullptr};
  EventCallback callback_;
};

static bool registered = []() {
  MonitorFactory::registerFactory(
      MonitorType::ALIVE,
      [](const std::string& name,
         const MonitorConfig& cfg) -> std::unique_ptr<IMonitor> {
        return std::make_unique<AliveMonitor>(name, cfg);
      });
  return true;
}();

}  // namespace phm
}  // namespace faw