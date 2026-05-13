#ifndef FAW_PHM_SUPERVISED_ENTITY_H
#define FAW_PHM_SUPERVISED_ENTITY_H

/// @file supervised_entity.h
/// @brief 监督实体：管理一组 Monitor，维护实体级状态机

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "faw/phm/health_channel.h"
#include "faw/phm/monitor.h"
#include "faw/phm/types.h"

namespace faw {
namespace phm {

/// 监督实体（Supervised Entity）
///
/// 一个 SE 代表一个被监督的软件单元（进程/线程组），包含多个 Monitor。
/// SE 维护自身状态机，根据各 Monitor 上报结果进行状态聚合和转换。
class SupervisedEntity {
 public:
  /// 构造 SE
  /// @param config SE 配置
  explicit SupervisedEntity(SEConfig config);

  ~SupervisedEntity();

  // 禁止拷贝
  SupervisedEntity(const SupervisedEntity&) = delete;
  SupervisedEntity& operator=(const SupervisedEntity&) = delete;

  // 允许移动
  SupervisedEntity(SupervisedEntity&&) noexcept;
  SupervisedEntity& operator=(SupervisedEntity&&) noexcept;

  /// 注册 Monitor
  /// @param monitor Monitor 唯一指针
  /// @return true 注册成功，false 名称重复
  bool registerMonitor(std::unique_ptr<IMonitor> monitor);

  /// 启动所有 Monitor
  /// @return true 全部启动成功
  bool start();

  /// 停止所有 Monitor
  void stop();

  /// 执行一次全量检测（遍历所有 Monitor 的 check()）
  /// @return 本次生成的事件列表
  std::vector<PhmEvent> checkAll();

  /// 重置实体（回到 INIT 状态，清空事件）
  void reset();

  /// 获取当前实体状态
  SeState getState() const;

  /// 获取实体名称
  const std::string& name() const noexcept { return config_.name; }

  /// 获取实体配置
  const SEConfig& config() const noexcept { return config_; }

  /// 获取健康通道
  /// @return 健康通道指针，如未配置返回 nullptr
  HealthChannel* getHealthChannel() noexcept;

  /// 获取所有 Monitor
  const std::vector<std::unique_ptr<IMonitor>>& monitors() const noexcept;

  /// 获取指定名称的 Monitor
  /// @param name Monitor 名称
  /// @return Monitor 指针，未找到返回 nullptr
  IMonitor* getMonitor(const std::string& name) const;

  /// 获取待处理事件并清空队列
  std::vector<PhmEvent> drainEvents();

  /// 状态变更回调
  using StateChangeCallback = std::function<void(
      const std::string& se_name, SeState old_state, SeState new_state)>;

  /// 设置状态变更回调
  void setStateChangeCallback(StateChangeCallback cb);

  /// 处理 Monitor 上报的事件
  void onMonitorEvent(const PhmEvent& event);

  /// 获取事件回调
  const StateChangeCallback& stateChangeCallback() const noexcept {
    return state_cb_;
  }

 private:
  SEConfig config_;
  std::vector<std::unique_ptr<IMonitor>> monitors_;
  std::unique_ptr<HealthChannel> health_channel_;
  std::vector<PhmEvent> pending_events_;
  StateChangeCallback state_cb_;
  SeState current_state_{SeState::INIT};

  // 状态机
  bool transitionTo(SeState new_state);
  bool isTransitionAllowed(SeState from, SeState to) const;

  // 处理 Monitor 事件结果
  void processMonitorResult(const PhmEvent& event);

  // 抖动抑制
  struct DebounceEntry {
    uint32_t consecutive_count{0};
    SeState target_state{SeState::INIT};
    std::chrono::steady_clock::time_point last_event;
  };
  std::map<std::string, DebounceEntry> debounce_map_;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_SUPERVISED_ENTITY_H