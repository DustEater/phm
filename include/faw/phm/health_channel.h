#ifndef FAW_PHM_HEALTH_CHANNEL_H
#define FAW_PHM_HEALTH_CHANNEL_H

/// @file health_channel.h
/// @brief 健康通道：用于被监督进程与 PHM 之间的跨进程活性通信

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace faw {
namespace phm {

struct HealthChannelStatus;

/// 健康通道
///
/// 被监督进程通过 HealthChannel 向 PHM 报告活性状态。
/// PHM 端通过检查计数器递增和超时来判断被监督进程是否存活。
///
/// 实现方式根据平台不同：
///   - Linux: 共享内存 (shm_open + mmap) 或 POSIX 消息队列
///   - QNX: 共享内存或 Pulse 消息
///
/// HealthChannel 设计为轻量级，单次操作应在微秒级完成。
class HealthChannel {
 public:
  /// 创建健康通道（PHM 侧，监控者）
  /// @param name 通道名称，对应 SE 名称
  /// @param timeout 存活超时时间
  explicit HealthChannel(std::string name, std::chrono::milliseconds timeout =
                                               std::chrono::milliseconds(5000));

  ~HealthChannel();

  // 禁止拷贝
  HealthChannel(const HealthChannel&) = delete;
  HealthChannel& operator=(const HealthChannel&) = delete;

  // 允许移动
  HealthChannel(HealthChannel&&) noexcept;
  HealthChannel& operator=(HealthChannel&&) noexcept;

  /// --- 以下方法由被监督进程调用 ---

  /// 上报 Alive 计数器递增
  /// @param alive_counter 被监督进程维护的自增计数器
  /// @return true 上报成功
  bool report(uint64_t alive_counter);

  /// 上报自定义状态数据
  /// @param key 键
  /// @param value 值
  /// @return true 上报成功
  bool reportStatus(const std::string& key, const std::string& value);

  /// --- 以下方法由 PHM 引擎/Monitor 调用 ---

  /// 获取最新 Alive 计数器值
  uint64_t getAliveCounter() const;

  /// 获取通道最新状态
  HealthChannelStatus getStatus() const;

  /// 检查是否存活（在 timeout 内计数器有更新）
  bool isAlive() const;

  /// 获取通道名称
  const std::string& name() const noexcept { return name_; }

  /// 设置存活超时时间
  void setTimeout(std::chrono::milliseconds timeout) noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
  std::string name_;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_HEALTH_CHANNEL_H