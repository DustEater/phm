#include "faw/phm/platform.h"

namespace faw {
namespace phm {

#if defined(PHM_PLATFORM_LINUX)

class LinuxPlatform : public Platform {
 public:
  bool processExists(pid_t pid) override;
  pid_t getProcessId(const std::string& process_name) override;
  pid_t startProcess(const std::string& path,
                     const std::vector<std::string>& args) override;
  bool stopProcess(pid_t pid, int sig) override;
  bool waitProcess(pid_t pid, std::chrono::milliseconds timeout) override;
  double getCpuUsage(pid_t pid) override;
  uint64_t getMemoryUsageBytes(pid_t pid) override;
  uint64_t getTotalSystemMemory() override;
  uint32_t getOpenFileCount(pid_t pid) override;
  double getSystemCpuLoad() override;
  std::string getHostname() override;
  uint64_t getSystemUptimeMs() override;
  bool checkThreadAlive(pthread_t thread,
                        std::chrono::milliseconds timeout) override;
};

// Linux 实现在 platform/linux/ 下
// 这里仅声明外部符号
extern LinuxPlatform g_linux_platform;

#elif defined(PHM_PLATFORM_QNX)

class QnxPlatform : public Platform {
 public:
  bool processExists(pid_t pid) override;
  pid_t getProcessId(const std::string& process_name) override;
  pid_t startProcess(const std::string& path,
                     const std::vector<std::string>& args) override;
  bool stopProcess(pid_t pid, int sig) override;
  bool waitProcess(pid_t pid, std::chrono::milliseconds timeout) override;
  double getCpuUsage(pid_t pid) override;
  uint64_t getMemoryUsageBytes(pid_t pid) override;
  uint64_t getTotalSystemMemory() override;
  uint32_t getOpenFileCount(pid_t pid) override;
  double getSystemCpuLoad() override;
  std::string getHostname() override;
  uint64_t getSystemUptimeMs() override;
  bool checkThreadAlive(pthread_t thread,
                        std::chrono::milliseconds timeout) override;
};

#endif

// =============================================================================
// 工厂方法实现
// =============================================================================

std::unique_ptr<Platform> Platform::create() {
#if defined(PHM_PLATFORM_LINUX)
  return std::make_unique<LinuxPlatform>();
#elif defined(PHM_PLATFORM_QNX)
  return std::make_unique<QnxPlatform>();
#else
  return nullptr;
#endif
}

}  // namespace phm
}  // namespace faw