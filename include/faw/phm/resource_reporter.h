/**
 * @file resource_reporter.h
 * @brief 进程资源上报器：定期采集并上报 CPU/内存/FD 等指标
 * */

#ifndef FAW_PHM_RESOURCE_REPORTER_H
#define FAW_PHM_RESOURCE_REPORTER_H

#include <chrono>
#include <atomic>
#include <memory>
#include <thread>

#include "faw/phm/health_channel.h"

namespace faw {
namespace phm {

/**
 * 进程资源上报器
 *
 * 定期采集当前进程的 CPU 使用率、内存占用、打开文件描述符数量，
 * 并通过 HealthChannel 上报给 PHM 引擎。
 * */
class ProcessResourceReporter {
 public:
  /**
   * @param hc HealthChannel 引用
   * @param interval 采集周期
   * */
  ProcessResourceReporter(HealthChannel& hc,
                          std::chrono::milliseconds interval = std::chrono::milliseconds(2000));
  ~ProcessResourceReporter();

  ProcessResourceReporter(const ProcessResourceReporter&) = delete;
  ProcessResourceReporter& operator=(const ProcessResourceReporter&) = delete;

  /**
   * 采集并上报资源指标
   * */
  void collectAndReport();

  /**
   * 启动周期性采集
   * */
  void start();

  /**
   * 停止周期性采集
   * */
  void stop();

 private:
  /**
   * 读取本进程 CPU 使用率
   * */
  double readSelfCpuUsage();

  /**
   * 读取本进程内存占用
   * */
  uint64_t readSelfMemoryBytes();

  /**
   * 读取本进程打开文件描述符数
   * */
  uint32_t readSelfOpenFdCount();

  HealthChannel& hc_;
  std::chrono::milliseconds interval_;
  std::atomic<bool> running_{false};
  std::unique_ptr<std::thread> worker_;

  uint64_t last_cpu_time_us_{0};
  uint64_t last_sys_time_us_{0};
  bool first_cpu_sample_{true};
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_RESOURCE_REPORTER_H