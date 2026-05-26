#include "faw/phm/phm_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "faw/phm/config_parser.h"
#include "faw/phm/data_reporter.h"
#include "faw/phm/logger.h"

namespace faw {
namespace phm {

// =============================================================================
// PhmEngine 内部实现
// =============================================================================

class PhmEngine::Impl {
 public:
  explicit Impl(const PhmConfig& cfg) : config_(cfg), running_(false) {
    Logger::instance().setLevel(Logger::INFO);
    if (!cfg.log_dir.empty()) {
      Logger::instance().setOutput(cfg.log_dir + "/phmd.log");
    }

    // 初始化数据上报器，使用 log_dir/data 作为数据存储根目录
    std::string data_dir =
        cfg.log_dir.empty() ? "/var/log/phmd/data" : cfg.log_dir + "/data";
    data_reporter_ = std::make_unique<DataReporter>(data_dir);
  }

  ~Impl() { stop(); }

  // ---- 配置 ----

  bool loadConfiguration(const std::string& xml_path) {
    try {
      auto result = ConfigParser::parseFile(xml_path);
      config_ = result.global;
      for (auto& cfg : result.entities) {
        if (!addSupervisedEntity(std::move(cfg))) {
          PHM_LOG_WARN("Failed to add entity from config");
        }
      }
      config_path_ = xml_path;
      return true;
    } catch (const std::exception& e) {
      PHM_LOG_ERROR("Config parse failed: %s", e.what());
      return false;
    }
  }

  bool addSupervisedEntity(SEConfig cfg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (entities_.find(cfg.name) != entities_.end()) {
      PHM_LOG_WARN("Entity '%s' already exists", cfg.name.c_str());
      return false;
    }

    auto entity = std::make_shared<SupervisedEntity>(std::move(cfg));

    // 注册状态变更回调
    entity->setStateChangeCallback(
        [this](const std::string& name, SeState old_state, SeState new_state) {
          onEntityStateChanged(name, old_state, new_state);
        });

    entities_[entity->name()] = entity;
    PHM_LOG_INFO("Entity '%s' registered", entity->name().c_str());
    return true;
  }

  bool registerEntity(std::shared_ptr<SupervisedEntity> entity) {
    if (!entity) return false;
    std::lock_guard<std::mutex> lock(mutex_);

    if (entities_.find(entity->name()) != entities_.end()) {
      return false;
    }

    entity->setStateChangeCallback(
        [this](const std::string& name, SeState old_state, SeState new_state) {
          onEntityStateChanged(name, old_state, new_state);
        });

    entities_[entity->name()] = std::move(entity);
    return true;
  }

  bool removeEntity(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entities_.find(name);
    if (it == entities_.end()) return false;

    it->second->stop();
    entities_.erase(it);
    PHM_LOG_INFO("Entity '%s' removed", name.c_str());
    return true;
  }

  // ---- 生命周期 ----

  bool start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return false;

    // 启动所有 SE
    for (auto& [name, entity] : entities_) {
      if (!entity->config().enabled) continue;
      entity->start();
      PHM_LOG_INFO("Entity '%s' started", name.c_str());
    }

    running_ = true;
    worker_ = std::thread(&Impl::tickLoop, this);
    PHM_LOG_INFO("PHM Engine started (tick=%lldms)",
                 (long long)config_.tick_interval.count());
    return true;
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) return;
      running_ = false;
    }

    if (worker_.joinable()) {
      worker_.join();
    }

    // 停止所有 SE
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, entity] : entities_) {
      entity->stop();
    }
    PHM_LOG_INFO("PHM Engine stopped");
  }

  bool isRunning() const noexcept { return running_; }

  // ---- 查询 ----

  SeState getGlobalState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SeState worst = SeState::RUNNING;
    for (const auto& [name, entity] : entities_) {
      SeState s = entity->getState();
      if (static_cast<uint8_t>(s) > static_cast<uint8_t>(worst)) {
        worst = s;
      }
    }
    return worst;
  }

  std::shared_ptr<SupervisedEntity> getEntity(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entities_.find(name);
    return it != entities_.end() ? it->second : nullptr;
  }

  std::vector<std::string> getEntityNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(entities_.size());
    for (const auto& [name, _] : entities_) {
      names.push_back(name);
    }
    return names;
  }

  EngineStats getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    EngineStats stats;
    stats.total_entities = entities_.size();
    stats.global_state = getGlobalState();

    size_t monitor_count = 0;
    for (const auto& [name, entity] : entities_) {
      monitor_count += entity->monitors().size();
      if (entity->getState() != SeState::INIT) {
        stats.active_entities++;
      }
    }
    stats.total_monitors = monitor_count;
    stats.uptime_ms = uptime_ms_.load();
    return stats;
  }

  std::vector<PhmEvent> getAllEvents(Severity min_severity) const {
    // 在实际实现中，应有全局事件队列
    // 此处简化：遍历所有 SE 收集事件
    return {};
  }

  void acknowledgeEvents(const std::vector<std::string>& event_ids) {
    // 在实际实现中，从全局事件队列移除已确认事件
  }

  bool reloadConfiguration() {
    if (config_path_.empty()) {
      PHM_LOG_WARN("No config path set, cannot reload");
      return false;
    }
    return loadConfiguration(config_path_);
  }

  void setGlobalStateCallback(GlobalStateCallback cb) {
    global_state_cb_ = std::move(cb);
  }

 private:
  void tickLoop() {
    auto interval = config_.tick_interval;
    auto start_time = std::chrono::steady_clock::now();

    while (running_) {
      auto tick_start = std::chrono::steady_clock::now();

      // 收集所有 SE 的快照
      std::vector<std::shared_ptr<SupervisedEntity>> snapshot;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, entity] : entities_) {
          snapshot.push_back(entity);
        }
      }

      // 遍历所有 SE 执行检测（不在锁内执行，避免长时间持有锁）
      for (auto& entity : snapshot) {
        auto events = entity->checkAll();
        if (!events.empty()) {
          PHM_LOG_DEBUG("Entity '%s' generated %zu events",
                        entity->name().c_str(), events.size());
        }

        // 收集监控数据上报
        collectAndReport(entity);

        // 检查自动重启
        if (entity->getState() == SeState::FATAL &&
            entity->config().auto_restart) {
          PHM_LOG_WARN("Auto-restarting entity '%s'", entity->name().c_str());
          entity->reset();
          entity->start();
        }
      }

      // 更新运行时间
      uptime_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();

      // 精确睡眠到下一个 tick
      auto elapsed = std::chrono::steady_clock::now() - tick_start;
      auto sleep_time = interval - elapsed;
      if (sleep_time > std::chrono::milliseconds(0)) {
        std::this_thread::sleep_for(sleep_time);
      }
    }
  }

  void collectAndReport(const std::shared_ptr<SupervisedEntity>& entity) {
    if (!data_reporter_) return;

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

    for (const auto& monitor : entity->monitors()) {
      DataRecord record;
      record.se_name = entity->name();
      record.monitor_name = monitor->name();
      record.monitor_type =
          ConfigParser::monitorTypeToString(monitor->getType());
      record.state = monitor->getState();
      record.timestamp_ms = now_ms;

      // 根据 Monitor 类型收集特有指标
      switch (monitor->getType()) {
        case MonitorType::RESOURCE: {
          auto m = monitor->collectMetrics();
          record.metrics["cpu_usage"] = m["cpu_usage"];
          record.metrics["memory_bytes"] = m["memory_bytes"];
          record.metrics["fd_count"] = m["fd_count"];
          break;
        }
        case MonitorType::ALIVE: {
          auto m = monitor->collectMetrics();
          record.metrics["alive_counter"] = m["alive_counter"];
          break;
        }
        case MonitorType::PROCESS_LIFECYCLE: {
          record.metrics["running"] =
              (monitor->getState() != SeState::FATAL) ? 1.0 : 0.0;
          break;
        }
        default:
          break;
      }

      // 将 PhmEvent 中的 metadata 转为 raw_data
      // （简化处理：事件数据通过事件队列另行传递，此处仅上报采样指标）
      data_reporter_->report(record);
    }
  }

  void onEntityStateChanged(const std::string& name, SeState old_state,
                            SeState new_state) {
    PHM_LOG_INFO("Entity '%s' state: %d -> %d", name.c_str(),
                 static_cast<int>(old_state), static_cast<int>(new_state));

    // 检查全局状态是否变化
    SeState old_global = getGlobalState();
    SeState new_global = getGlobalState();  // 重新计算

    if (old_global != new_global && global_state_cb_) {
      global_state_cb_(old_global, new_global);
    }
  }

  PhmConfig config_;
  std::string config_path_;
  std::atomic<bool> running_{false};
  std::atomic<uint64_t> uptime_ms_{0};
  std::thread worker_;
  mutable std::mutex mutex_;
  std::map<std::string, std::shared_ptr<SupervisedEntity>> entities_;
  GlobalStateCallback global_state_cb_;
  std::unique_ptr<DataReporter> data_reporter_;
};

// =============================================================================
// PhmEngine 公共接口：代理到 Impl
// =============================================================================

PhmEngine::PhmEngine(PhmConfig config)
    : impl_(std::make_unique<Impl>(config)), config_(std::move(config)) {}

PhmEngine::~PhmEngine() = default;

bool PhmEngine::loadConfiguration(const std::string& xml_path) {
  return impl_->loadConfiguration(xml_path);
}

bool PhmEngine::addSupervisedEntity(SEConfig cfg) {
  return impl_->addSupervisedEntity(std::move(cfg));
}

bool PhmEngine::registerEntity(std::shared_ptr<SupervisedEntity> entity) {
  return impl_->registerEntity(std::move(entity));
}

bool PhmEngine::removeEntity(const std::string& name) {
  return impl_->removeEntity(name);
}

bool PhmEngine::start() { return impl_->start(); }
void PhmEngine::stop() { impl_->stop(); }
bool PhmEngine::isRunning() const noexcept { return impl_->isRunning(); }

SeState PhmEngine::getGlobalState() const { return impl_->getGlobalState(); }

std::shared_ptr<SupervisedEntity> PhmEngine::getEntity(
    const std::string& name) const {
  return impl_->getEntity(name);
}

std::vector<std::string> PhmEngine::getEntityNames() const {
  return impl_->getEntityNames();
}

EngineStats PhmEngine::getStats() const { return impl_->getStats(); }

std::vector<PhmEvent> PhmEngine::getAllEvents(Severity min_severity) const {
  return impl_->getAllEvents(min_severity);
}

void PhmEngine::acknowledgeEvents(const std::vector<std::string>& event_ids) {
  impl_->acknowledgeEvents(event_ids);
}

bool PhmEngine::reloadConfiguration() { return impl_->reloadConfiguration(); }

void PhmEngine::setGlobalStateCallback(GlobalStateCallback cb) {
  impl_->setGlobalStateCallback(std::move(cb));
}

}  // namespace phm
}  // namespace faw