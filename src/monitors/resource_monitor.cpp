#include <atomic>
#include <chrono>
#include <string>

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
      // 尝试重新发现进程
      if (!process_name_.empty()) {
        pid_ = platform_->getProcessId(process_name_);
      }
      return event;
    }

    // 检查进程是否仍存在
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

    // CPU 使用率
    double cpu = platform_->getCpuUsage(pid_);
    uint64_t mem = platform_->getMemoryUsageBytes(pid_);

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
  EventCallback callback_;

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