#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "faw/phm/health_channel.h"
#include "faw/phm/logger.h"
#include "faw/phm/monitor.h"
#include "faw/phm/platform.h"

namespace faw {
namespace phm {

namespace {

bool checkThreadByName(const std::string& thread_name) {
  DIR* dir = opendir("/proc/self/task");
  if (!dir) return false;

  bool found = false;
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.') continue;

    std::string comm_path = std::string("/proc/self/task/") + entry->d_name + "/comm";
    int fd = open(comm_path.c_str(), O_RDONLY);
    if (fd < 0) continue;

    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) continue;
    buf[n] = '\0';

    std::string comm(buf);
    size_t newline = comm.find('\n');
    if (newline != std::string::npos) {
      comm.resize(newline);
    }

    if (comm == thread_name) {
      pid_t tid = static_cast<pid_t>(std::stoul(entry->d_name));
      int ret = tgkill(getpid(), tid, 0);
      if (ret == 0 || errno != ESRCH) {
        found = true;
        break;
      }
    }
  }
  closedir(dir);
  return found;
}

}  // namespace

/// 死锁/挂死检测器
///
/// 通过定期检测被监督线程的响应性来判断是否发生死锁或挂死。
/// 实现原理：在 HealthChannel 上写入标记，然后检查标记是否被被监督进程响应。
///
/// 区别于 AliveMonitor：
///   - AliveMonitor 检查 AliveCounter 递增（应用层心跳）
///   - DeadlockMonitor 检查线程级别的响应性（系统级检测）
class DeadlockMonitor : public IMonitor {
 public:
  DeadlockMonitor(const std::string& name, const MonitorConfig& cfg)
      : name_(name), config_(cfg), state_(SeState::INIT), last_check_ok_(true) {
    auto it = cfg.params.find("thread_name");
    if (it != cfg.params.end()) thread_name_ = it->second;
  }

  bool start() override {
    platform_ = Platform::create();
    if (!platform_) return false;

    state_ = SeState::RUNNING;
    last_check_ok_ = true;
    last_check_time_ = std::chrono::steady_clock::now();

    PHM_LOG_INFO("DeadlockMonitor '%s' started", name_.c_str());
    return true;
  }

  void stop() override {
    platform_.reset();
    state_ = SeState::INIT;
  }

  void reset() override {
    state_ = SeState::INIT;
    last_check_ok_ = true;
  }

  PhmEvent check() override {
    PhmEvent event;
    if (!platform_) return event;

    auto now = std::chrono::steady_clock::now();

    // 检查是否达到检测间隔
    if (now - last_check_time_ < config_.interval) {
      return event;  // 未到检测时间
    }

    last_check_time_ = now;

    // 通过 HealthChannel 检测挂死状态
    // 被监督进程应定期更新 HealthChannel，如果长时间未更新则判定为挂死

    bool alive = true;
    if (health_channel_) {
      alive = health_channel_->isAlive();
    } else {
      if (!thread_name_.empty()) {
        alive = checkThreadByName(thread_name_);
      }
    }

    if (!alive && last_check_ok_) {
      // 从正常到挂死
      last_check_ok_ = false;
      state_ = SeState::ERROR;

      event.id = generateId();
      event.source_monitor = name_;
      event.severity = Severity::ERROR;
      event.state = SeState::ERROR;
      event.message =
          "Deadlock or hang detected on thread '" + thread_name_ + "'";
      event.timestamp = std::chrono::system_clock::now();
      event.metadata["thread"] = thread_name_;
      event.metadata["timeout_ms"] = std::to_string(config_.timeout.count());
    } else if (alive && !last_check_ok_) {
      // 从挂死恢复
      last_check_ok_ = true;
      state_ = SeState::RUNNING;
    }

    return event;
  }

  SeState getState() const override { return state_; }
  MonitorType getType() const noexcept override {
    return MonitorType::DEADLOCK;
  }

  void configure(const MonitorConfig& cfg) override {
    config_ = cfg;
    auto it = cfg.params.find("thread_name");
    if (it != cfg.params.end()) thread_name_ = it->second;
  }

  const MonitorConfig& getConfig() const noexcept override { return config_; }
  const std::string& name() const noexcept override { return name_; }

  void setEventCallback(EventCallback cb) override {
    callback_ = std::move(cb);
  }

  void setHealthChannel(HealthChannel* hc) { health_channel_ = hc; }

 private:
  static std::string generateId() {
    static std::atomic<uint64_t> counter{0};
    return "phm_mon_dead_" + std::to_string(++counter) + "_" +
           std::to_string(
               std::chrono::system_clock::now().time_since_epoch().count());
  }

  std::string name_;
  MonitorConfig config_;
  std::string thread_name_;
  std::atomic<SeState> state_;
  bool last_check_ok_;
  std::chrono::steady_clock::time_point last_check_time_;
  HealthChannel* health_channel_{nullptr};
  std::unique_ptr<Platform> platform_;
  EventCallback callback_;
};

static bool registered = []() {
  MonitorFactory::registerFactory(
      MonitorType::DEADLOCK,
      [](const std::string& name,
         const MonitorConfig& cfg) -> std::unique_ptr<IMonitor> {
        return std::make_unique<DeadlockMonitor>(name, cfg);
      });
  return true;
}();

}  // namespace phm
}  // namespace faw