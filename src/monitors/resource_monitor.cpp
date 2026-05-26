#include <atomic>
#include <chrono>
#include <string>

#include "faw/phm/health_channel.h"
#include "faw/phm/logger.h"
#include "faw/phm/monitor.h"
#include "faw/phm/platform.h"

namespace faw {
namespace phm {

/// 资源监控器
///
/// 监控目标进程的 CPU 使用率和内存占用。
/// 支持双阈值配置（warn_threshold / error_threshold）。
class ResourceMonitor : public IMonitor {
 public:
  ResourceMonitor(const std::string& name, const MonitorConfig& cfg)
      : name_(name),
        config_(cfg),
        state_(SeState::INIT),
        pid_(-1),
        last_cpu_time_(0),
        last_sys_time_{} {
    auto it = cfg.params.find("process_name");
    if (it != cfg.params.end()) process_name_ = it->second;
  }

  bool start() override {
    platform_ = Platform::create();
    if (!platform_) return false;

    if (!process_name_.empty()) {
      pid_ = platform_->getProcessId(process_name_);
    }

    if (pid_ > 0) {
      state_ = SeState::RUNNING;
    }

    // 初始化 CPU 时间基线
    if (pid_ > 0) {
      last_cpu_time_ = getProcessCpuTime();
      last_sys_time_ = std::chrono::steady_clock::now();
    }

    PHM_LOG_INFO("ResourceMonitor '%s' started (pid=%d)", name_.c_str(), pid_);
    return true;
  }

  void stop() override {
    platform_.reset();
    state_ = SeState::INIT;
  }

  void reset() override {
    state_ = SeState::INIT;
    pid_ = -1;
    last_cpu_time_ = 0;
  }

  PhmEvent check() override {
    PhmEvent event;
    if (!platform_ || pid_ <= 0) {
      if (!process_name_.empty()) {
        pid_ = platform_->getProcessId(process_name_);
      }
      return event;
    }

    if (!platform_->processExists(pid_)) {
      pid_ = -1;
      state_ = SeState::ERROR;
      event.id = generateId();
      event.source_monitor = name_;
      event.severity = Severity::ERROR;
      event.state = SeState::ERROR;
      event.message = "Process no longer exists (resource monitor)";
      event.timestamp = std::chrono::system_clock::now();
      return event;
    }

    auto getMetricWithFallback = [this](const std::string& key,
                                        auto osFn, double& last_val) -> double {
      if (health_channel_) {
        auto metrics = health_channel_->getMetrics();
        auto it = metrics.find(key);
        if (it != metrics.end()) {
          last_val = it->second;
          return it->second;
        }
      }

      double val = osFn();
      if (val > 0.0 || key == "fd_count") {
        last_val = val;
        return val;
      }

      return last_val;
    };

    double cpu = getMetricWithFallback(
        "cpu",
        [this]() { return platform_->getCpuUsage(pid_); },
        last_cpu_);

    uint64_t mem = static_cast<uint64_t>(getMetricWithFallback(
        "memory_bytes",
        [this]() { return static_cast<double>(platform_->getMemoryUsageBytes(pid_)); },
        last_memory_));

    double fd = getMetricWithFallback(
        "fd_count",
        [this]() { return static_cast<double>(platform_->getOpenFileCount(pid_)); },
        last_fd_);
    (void)fd;

    // 检查 CPU 阈值
    if (config_.error_threshold > 0.0 && cpu >= config_.error_threshold) {
      state_ = SeState::ERROR;
      event.id = generateId();
      event.source_monitor = name_;
      event.severity = Severity::ERROR;
      event.state = SeState::ERROR;
      event.message = "CPU usage " + std::to_string(cpu) +
                      "% exceeds error threshold " +
                      std::to_string(config_.error_threshold) + "%";
      event.timestamp = std::chrono::system_clock::now();
      event.metadata["cpu_percent"] = std::to_string(cpu);
      event.metadata["memory_bytes"] = std::to_string(mem);
    } else if (config_.warn_threshold > 0.0 && cpu >= config_.warn_threshold) {
      state_ = SeState::WARN;
      event.id = generateId();
      event.source_monitor = name_;
      event.severity = Severity::WARNING;
      event.state = SeState::WARN;
      event.message = "CPU usage " + std::to_string(cpu) +
                      "% exceeds warn threshold " +
                      std::to_string(config_.warn_threshold) + "%";
      event.timestamp = std::chrono::system_clock::now();
      event.metadata["cpu_percent"] = std::to_string(cpu);
      event.metadata["memory_bytes"] = std::to_string(mem);
    } else if (state_ != SeState::RUNNING) {
      state_ = SeState::RUNNING;
    }

    return event;
  }

  SeState getState() const override { return state_; }
  MonitorType getType() const noexcept override {
    return MonitorType::RESOURCE;
  }

  void configure(const MonitorConfig& cfg) override {
    config_ = cfg;
    auto it = cfg.params.find("process_name");
    if (it != cfg.params.end()) process_name_ = it->second;
  }

  const MonitorConfig& getConfig() const noexcept override { return config_; }
  const std::string& name() const noexcept override { return name_; }

  void setEventCallback(EventCallback cb) override {
    callback_ = std::move(cb);
  }

  void setHealthChannel(HealthChannel* hc) { health_channel_ = hc; }

  std::map<std::string, double> collectMetrics() override {
    std::map<std::string, double> m;
    m["cpu_usage"] = last_cpu_;
    m["memory_bytes"] = last_memory_;
    m["fd_count"] = last_fd_;
    return m;
  }

 private:
  static std::string generateId() {
    static std::atomic<uint64_t> counter{0};
    return "phm_mon_res_" + std::to_string(++counter) + "_" +
           std::to_string(
               std::chrono::system_clock::now().time_since_epoch().count());
  }

  uint64_t getProcessCpuTime() {
    // 简化：从 /proc/[pid]/stat 读取 utime+stime
    // 实际实现在 platform 层
    return 0;
  }

  std::string name_;
  MonitorConfig config_;
  std::string process_name_;
  std::atomic<SeState> state_;
  pid_t pid_;
  std::unique_ptr<Platform> platform_;
  HealthChannel* health_channel_{nullptr};
  EventCallback callback_;

  double last_cpu_{0.0};
  double last_memory_{0.0};
  double last_fd_{0.0};

  uint64_t last_cpu_time_;
  std::chrono::steady_clock::time_point last_sys_time_;
};

static bool registered = []() {
  MonitorFactory::registerFactory(
      MonitorType::RESOURCE,
      [](const std::string& name,
         const MonitorConfig& cfg) -> std::unique_ptr<IMonitor> {
        return std::make_unique<ResourceMonitor>(name, cfg);
      });
  return true;
}();

}  // namespace phm
}  // namespace faw