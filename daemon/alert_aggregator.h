/**
 * @file alert_aggregator.h
 * @brief 告警聚合器 —— 多 SE 告警汇总、去重、降噪
 * */

#ifndef FAW_PHM_ALERT_AGGREGATOR_H
#define FAW_PHM_ALERT_AGGREGATOR_H

#include <memory>
#include <string>
#include <vector>

#include "faw/phm/types.h"

namespace faw {
namespace phm {

/**
 * 告警聚合器
 *
 * 负责：
 * 1. 从多个 SE 收集告警事件
 * 2. 相同告警抑制（N 分钟内重复不上报）
 * 3. 告警升级（长时间未恢复的 WARNING 升级为 ERROR）
 * 4. 告警摘要生成
 * */
class AlertAggregator {
 public:
  AlertAggregator();
  ~AlertAggregator();

  /**
   * 添加事件
   * */
  void addEvent(const PhmEvent& event);

  /**
   * 获取需要上报的告警（已去重和降噪）
   * */
  std::vector<PhmEvent> getAggregatedAlerts();

  /**
   * 设置告警抑制时间
   * */
  void setSuppressionDuration(std::chrono::minutes duration);

  /**
   * 设置告警升级时间
   * */
  void setEscalationDuration(std::chrono::minutes duration);

  /**
   * 清空
   * */
  void clear();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_ALERT_AGGREGATOR_H