#include <algorithm>
#include <cassert>

#include "faw/phm/supervised_entity.h"

namespace faw {
namespace phm {

// 将 Severity 映射到 SeState
static SeState seStateFromSeverity(Severity sev) {
  switch (sev) {
    case Severity::INFO:
      return SeState::RUNNING;
    case Severity::WARNING:
      return SeState::WARN;
    case Severity::ERROR:
      return SeState::ERROR;
    case Severity::CRITICAL:
      return SeState::ERROR;
    case Severity::FATAL:
      return SeState::FATAL;
    default:
      return SeState::RUNNING;
  }
}

// =============================================================================
// 状态转换表
//
// 允许的转换路径（非严格状态机）：
//   INIT     -> RUNNING, SUSPECT
//   RUNNING  -> SUSPECT, WARN, ERROR, FATAL
//   SUSPECT  -> RUNNING, WARN, ERROR
//   WARN     -> RUNNING, ERROR
//   ERROR    -> RUNNING, FATAL
//   FATAL    -> (终态，无合法转换)
// =============================================================================

static bool g_transition_table[6][6] = {
    // from\to: INIT  RUN   SUSP  WARN  ERR   FATAL
    /* INIT */ {false, true, true, false, false, false},
    /* RUNNING */ {false, false, true, true, true, true},
    /* SUSPECT */ {false, true, false, true, true, false},
    /* WARN */ {false, true, false, false, true, false},
    /* ERROR */ {false, true, false, false, false, true},
    /* FATAL */ {false, false, false, false, false, false},
};

static uint8_t seStateToIndex(SeState s) { return static_cast<uint8_t>(s); }

bool SupervisedEntity::isTransitionAllowed(SeState from, SeState to) const {
  uint8_t fi = seStateToIndex(from);
  uint8_t ti = seStateToIndex(to);
  if (fi >= 6 || ti >= 6) return false;
  return g_transition_table[fi][ti];
}

bool SupervisedEntity::transitionTo(SeState new_state) {
  if (!isTransitionAllowed(current_state_, new_state)) {
    return false;
  }

  SeState old_state = current_state_;
  current_state_ = new_state;

  // 触发回调
  if (state_cb_) {
    state_cb_(config_.name, old_state, new_state);
  }

  return true;
}

// =============================================================================
// 构造函数 / 析构函数
// =============================================================================

SupervisedEntity::SupervisedEntity(SEConfig config)
    : config_(std::move(config)) {
  // 创建健康通道
  health_channel_ =
      std::make_unique<HealthChannel>(config_.name, config_.alive_timeout);

  // 初始化抖动抑制表
  for (const auto& mc : config_.monitors) {
    if (mc.enabled) {
      debounce_map_[mc.type == MonitorType::PROCESS_LIFECYCLE
                        ? "process_lifecycle"
                    : mc.type == MonitorType::RESOURCE ? "resource"
                    : mc.type == MonitorType::DEADLOCK ? "deadlock"
                    : mc.type == MonitorType::ALIVE    ? "alive"
                                                       : "custom"] =
          DebounceEntry{};
    }
  }
}

SupervisedEntity::~SupervisedEntity() { stop(); }

SupervisedEntity::SupervisedEntity(SupervisedEntity&&) noexcept = default;
SupervisedEntity& SupervisedEntity::operator=(SupervisedEntity&&) noexcept =
    default;

// =============================================================================
// Monitor 管理
// =============================================================================

bool SupervisedEntity::registerMonitor(std::unique_ptr<IMonitor> monitor) {
  if (!monitor) return false;

  // 检查名称是否重复
  for (const auto& m : monitors_) {
    if (m->name() == monitor->name()) {
      return false;
    }
  }

  // 设置事件回调
  auto weak_this = std::weak_ptr<SupervisedEntity>(
      std::shared_ptr<SupervisedEntity>(this, [](auto*) {}));
  monitor->setEventCallback(
      [this](const PhmEvent& event) { onMonitorEvent(event); });

  monitors_.push_back(std::move(monitor));
  return true;
}

bool SupervisedEntity::start() {
  bool all_ok = true;
  for (auto& m : monitors_) {
    if (!m->start()) {
      all_ok = false;
    }
  }

  if (all_ok && current_state_ == SeState::INIT) {
    transitionTo(SeState::RUNNING);
  }

  return all_ok;
}

void SupervisedEntity::stop() {
  for (auto& m : monitors_) {
    m->stop();
  }
}

void SupervisedEntity::reset() {
  stop();
  pending_events_.clear();
  debounce_map_.clear();
  current_state_ = SeState::INIT;

  for (auto& m : monitors_) {
    m->reset();
  }
}

// =============================================================================
// 检测执行
// =============================================================================

std::vector<PhmEvent> SupervisedEntity::checkAll() {
  std::vector<PhmEvent> events;

  for (auto& m : monitors_) {
    PhmEvent evt = m->check();

    // 空事件（id 为空）表示无需处理
    if (evt.id.empty()) {
      continue;
    }

    events.push_back(evt);
    processMonitorResult(evt);
  }

  return events;
}

/// 处理 Monitor 上报的事件
void SupervisedEntity::processMonitorResult(const PhmEvent& event) {
  // 查找 Monitor 配置（通过类型字符串匹配）
  MonitorType event_type = MonitorType::CUSTOM;
  if (event.source_monitor == "process_lifecycle")
    event_type = MonitorType::PROCESS_LIFECYCLE;
  else if (event.source_monitor == "resource")
    event_type = MonitorType::RESOURCE;
  else if (event.source_monitor == "deadlock")
    event_type = MonitorType::DEADLOCK;
  else if (event.source_monitor == "alive")
    event_type = MonitorType::ALIVE;

  const MonitorConfig* mc_ptr = nullptr;
  for (const auto& cfg : config_.monitors) {
    if (cfg.type == event_type) {
      mc_ptr = &cfg;
      break;
    }
  }
  if (!mc_ptr) return;
  const auto& mc = *mc_ptr;

  uint32_t debounce = mc.debounce_count;
  if (debounce == 0) debounce = 1;  // 至少 1

  SeState target = seStateFromSeverity(event.severity);

  // 查找对应的抖动抑制条目
  auto key = event.source_monitor;
  auto& entry = debounce_map_[key];

  if (target == SeState::RUNNING) {
    // 恢复正常 -> 直接重置抖动计数器
    entry.consecutive_count = 0;
    entry.target_state = SeState::RUNNING;

    // 如果当前不是 RUNNING，尝试恢复
    if (current_state_ != SeState::RUNNING) {
      transitionTo(SeState::RUNNING);
    }
    return;
  }

  // 达到或超过阈值 -> 递增计数器
  if (seStateToIndex(target) > seStateToIndex(current_state_)) {
    entry.consecutive_count++;
    if (entry.consecutive_count >= debounce) {
      transitionTo(target);
      entry.consecutive_count = 0;
    } else {
      // 未达到阈值，先进入 SUSPECT
      if (current_state_ == SeState::RUNNING) {
        transitionTo(SeState::SUSPECT);
      }
    }
  }
}

// =============================================================================
// 辅助方法
// =============================================================================

SeState SupervisedEntity::getState() const { return current_state_; }

HealthChannel* SupervisedEntity::getHealthChannel() noexcept {
  return health_channel_.get();
}

const std::vector<std::unique_ptr<IMonitor>>& SupervisedEntity::monitors()
    const noexcept {
  return monitors_;
}

IMonitor* SupervisedEntity::getMonitor(const std::string& name) const {
  for (const auto& m : monitors_) {
    if (m->name() == name) {
      return m.get();
    }
  }
  return nullptr;
}

std::vector<PhmEvent> SupervisedEntity::drainEvents() {
  std::vector<PhmEvent> events;
  events.swap(pending_events_);
  return events;
}

void SupervisedEntity::setStateChangeCallback(StateChangeCallback cb) {
  state_cb_ = std::move(cb);
}

void SupervisedEntity::onMonitorEvent(const PhmEvent& event) {
  pending_events_.push_back(event);
  processMonitorResult(event);
}

}  // namespace phm
}  // namespace faw