#include "faw/phm/platform.h"

// QNX 平台适配桩代码
// 实现在 QNX Neutrino RTOS 上运行时编译和链接

#if defined(PHM_PLATFORM_QNX)

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>
#include <sys/sysmgr.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace faw {
namespace phm {

// =============================================================================
// QNX 辅助函数
// =============================================================================

/// 打开 procnto 文件描述符
static int openProcnto(pid_t pid) {
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%d/as", pid);
  return open(path, O_RDONLY);
}

// =============================================================================
// QnxPlatform 实现
// =============================================================================

bool QnxPlatform::processExists(pid_t pid) {
  if (pid <= 0) return false;
  // QNX: 使用 slm_query 或 procnto 检测
  // 简化: 使用 kill 0 探测
  return (kill(pid, 0) == 0);
}

pid_t QnxPlatform::getProcessId(const std::string& process_name) {
  // QNX: 遍历 procnto 查找进程
  DIR* proc_dir = opendir("/proc");
  if (!proc_dir) return -1;

  struct dirent* entry;
  pid_t found_pid = -1;

  while ((entry = readdir(proc_dir)) != nullptr) {
    char* end;
    pid_t pid = std::strtol(entry->d_name, &end, 10);
    if (*end != '\0') continue;

    // QNX: 读取进程名称
    int fd = openProcnto(pid);
    if (fd < 0) continue;

    procfs_info info;
    if (devctl(fd, DCMD_PROC_INFO, &info, sizeof(info), nullptr) == 0) {
      if (process_name == info.name) {
        found_pid = pid;
        close(fd);
        break;
      }
    }
    close(fd);
  }

  closedir(proc_dir);
  return found_pid;
}

pid_t QnxPlatform::startProcess(const std::string& path,
                                const std::vector<std::string>& args) {
  // QNX: 使用 spawn() 启动进程
  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(path.c_str()));
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid;
  int rc =
      posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv.data(), environ);
  if (rc != 0) return -1;

  return pid;
}

bool QnxPlatform::stopProcess(pid_t pid, int sig) {
  // QNX: 使用 SignalKill
  return (SignalKill(ND_LOCAL_NODE, pid, sig, 0, 0) == 0);
}

bool QnxPlatform::waitProcess(pid_t pid, std::chrono::milliseconds timeout) {
  int status;
  auto deadline = std::chrono::steady_clock::now() + timeout;

  while (std::chrono::steady_clock::now() < deadline) {
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) return true;
    if (result == -1) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

double QnxPlatform::getCpuUsage(pid_t pid) {
  // QNX: 通过 procnto DCMD_PROC_RUSAGE 获取 CPU 时间
  int fd = openProcnto(pid);
  if (fd < 0) return 0.0;

  procfs_rusage rusage;
  if (devctl(fd, DCMD_PROC_RUSAGE, &rusage, sizeof(rusage), nullptr) != 0) {
    close(fd);
    return 0.0;
  }
  close(fd);

  // 计算 CPU 使用率
  static auto last_time = std::chrono::steady_clock::now();
  static uint64_t last_cpu_time = 0;

  uint64_t cpu_time = rusage.ru_utime.nsec + rusage.ru_stime.nsec;
  auto now = std::chrono::steady_clock::now();
  auto elapsed_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time)
          .count();

  if (elapsed_ns <= 0 || last_cpu_time == 0) {
    last_cpu_time = cpu_time;
    last_time = now;
    return 0.0;
  }

  double cpu_percent = static_cast<double>(cpu_time - last_cpu_time) /
                       static_cast<double>(elapsed_ns) * 100.0;

  last_cpu_time = cpu_time;
  last_time = now;

  return std::min(cpu_percent, 100.0 * sysconf(_SC_NPROCESSORS_ONLN));
}

uint64_t QnxPlatform::getMemoryUsageBytes(pid_t pid) {
  // QNX: 通过 asinfo() 获取进程地址空间大小
  int fd = openProcnto(pid);
  if (fd < 0) return 0;

  procfs_asinfo asinfo;
  if (devctl(fd, DCMD_PROC_ASINFO, &asinfo, sizeof(asinfo), nullptr) != 0) {
    close(fd);
    return 0;
  }
  close(fd);

  return asinfo.resident_size;
}

uint64_t QnxPlatform::getTotalSystemMemory() {
  // QNX: 通过 sysmgr 或 asinfo 获取系统总内存
  uint64_t total = 0;
  // 简化：使用 sysconf
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages > 0 && page_size > 0) {
    total = static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
  }
  return total;
}

uint32_t QnxPlatform::getOpenFileCount(pid_t pid) {
  // QNX: 通过 procnto 获取文件描述符信息
  int fd = openProcnto(pid);
  if (fd < 0) return 0;

  procfs_fdinfo fdinfo;
  if (devctl(fd, DCMD_PROC_FDINFO, &fdinfo, sizeof(fdinfo), nullptr) != 0) {
    close(fd);
    return 0;
  }
  close(fd);

  return fdinfo.num_fds;
}

double QnxPlatform::getSystemCpuLoad() {
  // QNX: 使用 sysmgr 获取系统 CPU 负载
  // 简化实现
  return 0.0;
}

std::string QnxPlatform::getHostname() {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) == 0) {
    buf[sizeof(buf) - 1] = '\0';
    return buf;
  }
  return "unknown";
}

uint64_t QnxPlatform::getSystemUptimeMs() {
  // QNX: ClockTime 获取系统运行时间
  struct timespec ts;
  if (clock_gettime(CLOCK_UPTIME, &ts) == 0) {
    return static_cast<uint64_t>(ts.tv_sec) * 1000 +
           static_cast<uint64_t>(ts.tv_nsec) / 1000000;
  }
  return 0;
}

bool QnxPlatform::checkThreadAlive(pthread_t thread,
                                   std::chrono::milliseconds timeout) {
  // QNX: 使用 SyncCondTimedwait + pulse 实现线程存活检测
  // 简化: 使用 pthread_kill 0 探测
  return (pthread_kill(thread, 0) == 0);
}

}  // namespace phm
}  // namespace faw

#endif  // PHM_PLATFORM_QNX