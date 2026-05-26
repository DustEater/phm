#ifndef FAW_PHM_PHM_ENGINE_H
#define FAW_PHM_PHM_ENGINE_H

/**
 * @file phm_engine.h
 * @brief PHM 引擎：全局入口，管理所有 SE，聚合全局状态
 * */

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "faw/phm/supervised_entity.h"
#include "faw/phm/types.h"

namespace faw {
namespace phm {

/**
 * PHM 引擎
 *
 * 核心入口点，负责：
 * 1. 管理所有 SupervisedEntity 的生命周期
 * 2. 按 tick_interval 周期调度各 SE 的检测
 * 3. 聚合全局健康状态
 * 4. 提供运行时配置重载能力
 *
 * 线程安全：所有公共方法支持多线程并发调用。
 * */
class PhmEngine {
 public:
  /**
   * 构造引擎
   * @param config PHM 全局配置
   * */
  explicit PhmEngine(PhmConfig config = PhmConfig{});

  ~PhmEngine();

  // 禁止拷贝
  PhmEngine(const PhmEngine&) = delete;
  PhmEngine& operator=(const PhmEngine&) = delete;

  // ===== 配置 =====

  /**
   * 从 XML 文件加载 SE 配置
   * @param xml_path XML 配置文件路径
   * @return true 加载成功
   * */
  bool loadConfiguration(const std::string& xml_path);

  /**
   * 直接添加 SE 配置（不通过 XML）
   * @param cfg SE 配置
   * @return true 添加成功
   * */
  bool addSupervisedEntity(SEConfig cfg);

  /**
   * 注册已构建的 SE 对象
   * @param entity SE 共享指针
   * @return true 注册成功
   * */
  bool registerEntity(std::shared_ptr<SupervisedEntity> entity);

  /**
   * 移除 SE
   * @param name SE 名称
   * @return true 移除成功
   * */
  bool removeEntity(const std::string& name);

  // ===== 生命周期 =====

  /**
   * 启动引擎（开始调度所有 SE 的检测）
   * @return true 启动成功
   * */
  bool start();

  /**
   * 停止引擎
   * */
  void stop();

  /**
   * 引擎是否运行中
   * */
  bool isRunning() const noexcept;

  // ===== 查询 =====

  /**
   * 获取全局聚合状态（取最严重状态）
   * */
  SeState getGlobalState() const;

  /**
   * 获取 SE 对象
   * @param name SE 名称
   * @return SE 共享指针，未找到返回 nullptr
   * */
  std::shared_ptr<SupervisedEntity> getEntity(const std::string& name) const;

  /**
   * 获取所有 SE 名称
   * */
  std::vector<std::string> getEntityNames() const;

  /**
   * 获取引擎统计信息
   * */
  EngineStats getStats() const;

  /**
   * 获取所有待处理事件（按严重级别过滤）
   * @param min_severity 最低严重级别
   * @return 事件列表
   * */
  std::vector<PhmEvent> getAllEvents(
      Severity min_severity = Severity::INFO) const;

  /**
   * 确认事件（从待处理队列移除）
   * @param event_ids 事件 ID 列表
   * */
  void acknowledgeEvents(const std::vector<std::string>& event_ids);

  // ===== 运行时操作 =====

  /**
   * 运行时重载配置
   * @return true 重载成功
   * */
  bool reloadConfiguration();

  /**
   * 获取配置
   * */
  const PhmConfig& config() const noexcept { return config_; }

  // ===== 回调 =====

  /**
   * 全局状态变更回调
   * */
  using GlobalStateCallback =
      std::function<void(SeState old_state, SeState new_state)>;
  void setGlobalStateCallback(GlobalStateCallback cb);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
  PhmConfig config_;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_PHM_ENGINE_H