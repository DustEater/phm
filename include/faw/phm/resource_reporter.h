#ifndef FAW_PHM_RESOURCE_REPORTER_H
#define FAW_PHM_RESOURCE_REPORTER_H

#include <chrono>
#include <atomic>
#include <memory>
#include <thread>

#include "faw/phm/health_channel.h"

namespace faw {
namespace phm {

class ProcessResourceReporter {
 public:
  ProcessResourceReporter(HealthChannel& hc,
                          std::chrono::milliseconds interval = std::chrono::milliseconds(2000));
  ~ProcessResourceReporter();

  ProcessResourceReporter(const ProcessResourceReporter&) = delete;
  ProcessResourceReporter& operator=(const ProcessResourceReporter&) = delete;

  void collectAndReport();

  void start();
  void stop();

 private:
  double readSelfCpuUsage();
  uint64_t readSelfMemoryBytes();
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