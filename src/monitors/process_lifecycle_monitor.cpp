#include <atomic>
#include <chrono>
#include <string>

#include "faw/phm/logger.h"
#include "faw/phm/monitor.h"
#include "faw/phm/platform.h"

namespace faw {
namespace phm {

/// 进程生命周期监控器
///
/// 监控目标进程的启动、运行和退出状态。
/// 支持按进程名和 PID 两种方式识别目标。
class ProcessLifecycleMonitor : public IMonitor {
 public:
  ProcessLifecycleMonitor(const std::string& name, const MonitorConfig& cfg)
      : name_(name), config_(cfg), state_(SeState::INIT), pid_(-1) {
    // 从配置参数中提取进程名和路径
    auto it = cfg.params.find("process_name");
    if (it != cfg.params.end()) {
      process_name_ = it->second;
    }
    it = cfg.params.find("process_path");
    if (it != cfg.params.end()) {
      process_path_ = it->second;
    }
  }

  bool start() override {
    platform_ = Platform::create();
    if (!platform_) {
      PHM_LOG_ERROR("Failed to create platform instance");
      return false;
    }

    // 如果配置了进程名，尝试查找 PID
    if (!process_name_.empty()) {
      pid_ = platform_->getProcessId(process_name_);
      if (pid_ > 0) {
        state_ = SeState::RUNNING;
      }
    }

    PHM_LOG_INFO("ProcessLifecycleMonitor '%s' started (pid=%d, name=%s)",
                 name_.c_str(), pid_, process_name_.c_str());
    return true;
  }

  void stop() override {
    platform_.reset();
    state_ = SeState::INIT;
  }

  void reset() override {
    state_ = SeState::INIT;
    pid_ = -1;
  }

  PhmEvent check() override {
    PhmEvent event;
    if (!platform_) return event;

    bool exists = false;
    if (pid_ > 0) {
      exists = platform_->processExists(pid_);
    } else if (!process_name_.empty()) {
      pid_ = platform_->getProcessId(process_name_);
      exists = (pid_ > 0);
    }

    if (!exists && state_ == SeState::RUNNING) {
      // 进程消失了！
      state_ = SeState::ERROR;
      event.id = generateId();
      event.source_se = "";
      event.source_monitor = name_;
      event.severity = Severity::ERROR;
      event.state = SeState::ERROR;
      event.message = "Process '" + process_name_ +
                      "' (pid=" + std::to_string(pid_) + ") has exited";
      event.timestamp = std::chrono::system_clock::now();
      event.metadata["process_name"] = process_name_;
      event.metadata["last_pid"] = std::to_string(pid_);
      pid_ = -1;
    } else if (exists && state_ == SeState::ERROR) {
      // 进程重新出现（可能是重启）
      state_ = SeState::RUNNING;
    }

    return event;
  }

  SeState getState() const override { return state_; }
  MonitorType getType() const noexcept override {
    return MonitorType::PROCESS_LIFECYCLE;
  }

  void configure(const MonitorConfig& cfg) override {
    config_ = cfg;
    auto it = cfg.params.find("process_name");
    if (it != cfg.params.end()) process_name_ = it->second;
    it = cfg.params.find("process_path");
    if (it != cfg.params.end()) process_path_ = it->second;
  }

  const MonitorConfig& getConfig() const noexcept override { return config_; }
  const std::string& name() const noexcept override { return name_; }

  void setEventCallback(EventCallback cb) override {
    callback_ = std::move(cb);
  }

 private:
  static std::string generateId() {
    static std::atomic<uint64_t> counter{0};
    return "phm_mon_proc_" + std::to_string(++counter) + "_" +
           std::to_string(
               std::chrono::system_clock::now().time_since_epoch().count());
  }

  std::string name_;
  MonitorConfig config_;
  std::string process_name_;
  std::string process_path_;
  std::atomic<SeState> state_;
  pid_t pid_;
  std::unique_ptr<Platform> platform_;
  EventCallback callback_;
};

/// 自动注册工厂
static bool registered = []() {
  MonitorFactory::registerFactory(
      MonitorType::PROCESS_LIFECYCLE,
      [](const std::string& name,
         const MonitorConfig& cfg) -> std::unique_ptr<IMonitor> {
        return std::make_unique<ProcessLifecycleMonitor>(name, cfg);
      });
  return true;
}();

}  // namespace phm
}  // namespace faw