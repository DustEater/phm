#ifndef FAW_PHM_MONITOR_H
#define FAW_PHM_MONITOR_H

/// @file monitor.h
/// @brief Monitor 抽象接口 —— 所有具体 Monitor 需实现此接口

#include <functional>
#include <memory>
#include <string>

#include "faw/phm/types.h"

namespace faw {
namespace phm {

/// Monitor 抽象接口
///
/// 所有监控器必须实现此接口。
/// 实现类应保持轻量，check() 方法应快速返回（毫秒级）。
/// 长时间操作应分解为多次 check() 调用。
class IMonitor {
 public:
  virtual ~IMonitor() = default;

  /// 启动监控（在单独调度前初始化资源）
  /// @return true 启动成功
  virtual bool start() = 0;

  /// 停止监控（释放资源）
  virtual void stop() = 0;

  /// 执行一次检测
  /// @return 本次检测生成的事件（可能为空事件，通过检查 id 是否为空判断）
  virtual PhmEvent check() = 0;

  /// 重置监控器状态
  virtual void reset() = 0;

  /// 获取当前状态
  virtual SeState getState() const = 0;

  /// 获取监控类型
  virtual MonitorType getType() const noexcept = 0;

  /// 加载配置
  /// @param cfg Monitor 配置
  virtual void configure(const MonitorConfig& cfg) = 0;

  /// 获取配置引用
  virtual const MonitorConfig& getConfig() const noexcept = 0;

  /// 获取监控器名称
  virtual const std::string& name() const noexcept = 0;

  /// 事件回调类型
  using EventCallback = std::function<void(const PhmEvent&)>;

  /// 设置事件回调（在 check() 中被调用）
  virtual void setEventCallback(EventCallback cb) = 0;
};

/// Monitor 工厂函数类型
using MonitorFactoryFunc = std::unique_ptr<IMonitor> (*)(
    const std::string& name, const MonitorConfig& cfg);

/// Monitor 工厂注册表（用于自定义 Monitor 扩展）
class MonitorFactory {
 public:
  /// 注册 Monitor 工厂
  /// @param type MonitorType
  /// @param factory 工厂函数
  static void registerFactory(MonitorType type, MonitorFactoryFunc factory);

  /// 创建 Monitor 实例
  /// @param type 类型
  /// @param name 名称
  /// @param cfg 配置
  /// @return Monitor 唯一指针，失败返回 nullptr
  static std::unique_ptr<IMonitor> create(MonitorType type,
                                          const std::string& name,
                                          const MonitorConfig& cfg);

  /// 获取已注册的所有类型
  static std::vector<MonitorType> registeredTypes();
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_MONITOR_H