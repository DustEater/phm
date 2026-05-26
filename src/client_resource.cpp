#include "faw/phm/resource_reporter.h"

#include <dirent.h>
#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <map>
#include <string>
#include <thread>

#include "faw/phm/logger.h"

namespace faw {
namespace phm {

ProcessResourceReporter::ProcessResourceReporter(
    HealthChannel& hc, std::chrono::milliseconds interval)
    : hc_(hc), interval_(interval) {}

ProcessResourceReporter::~ProcessResourceReporter() { stop(); }

void ProcessResourceReporter::collectAndReport() {
  double cpu = readSelfCpuUsage();
  uint64_t mem = readSelfMemoryBytes();
  uint32_t fd = readSelfOpenFdCount();

  std::map<std::string, double> metrics;
  metrics["cpu"] = cpu;
  metrics["memory_bytes"] = static_cast<double>(mem);
  metrics["fd_count"] = static_cast<double>(fd);

  hc_.reportMetrics(metrics);
}

void ProcessResourceReporter::start() {
  if (running_.exchange(true)) return;

  worker_ = std::make_unique<std::thread>([this]() {
    while (running_) {
      collectAndReport();
      std::this_thread::sleep_for(interval_);
    }
  });

  PHM_LOG_INFO("ProcessResourceReporter started (interval=%lldms)",
               (long long)interval_.count());
}

void ProcessResourceReporter::stop() {
  if (!running_.exchange(false)) return;

  if (worker_ && worker_->joinable()) {
    worker_->join();
  }
  worker_.reset();

  PHM_LOG_INFO("ProcessResourceReporter stopped");
}

double ProcessResourceReporter::readSelfCpuUsage() {
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) return 0.0;

  uint64_t cpu_time_us =
      static_cast<uint64_t>(usage.ru_utime.tv_sec) * 1000000 +
      static_cast<uint64_t>(usage.ru_utime.tv_usec) +
      static_cast<uint64_t>(usage.ru_stime.tv_sec) * 1000000 +
      static_cast<uint64_t>(usage.ru_stime.tv_usec);

  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
                 .count();

  if (first_cpu_sample_) {
    last_cpu_time_us_ = cpu_time_us;
    last_sys_time_us_ = static_cast<uint64_t>(now);
    first_cpu_sample_ = false;
    return 0.0;
  }

  uint64_t cpu_delta = cpu_time_us - last_cpu_time_us_;
  uint64_t time_delta = static_cast<uint64_t>(now) - last_sys_time_us_;

  last_cpu_time_us_ = cpu_time_us;
  last_sys_time_us_ = static_cast<uint64_t>(now);

  if (time_delta == 0) return 0.0;

  return (static_cast<double>(cpu_delta) / static_cast<double>(time_delta)) *
         100.0;
}

uint64_t ProcessResourceReporter::readSelfMemoryBytes() {
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;

  uint64_t rss_kb = static_cast<uint64_t>(usage.ru_maxrss);
  return rss_kb * 1024;
}

uint32_t ProcessResourceReporter::readSelfOpenFdCount() {
  uint32_t count = 0;

  std::string fd_path = "/proc/self/fd";
  DIR* dir = opendir(fd_path.c_str());
  if (!dir) return 0;

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_LNK ||
        entry->d_type == DT_UNKNOWN) {
      ++count;
    }
  }
  closedir(dir);

  return count > 2 ? count - 2 : 0;
}

}  // namespace phm
}  // namespace faw