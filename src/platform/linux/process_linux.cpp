#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "faw/phm/platform.h"

namespace faw {
namespace phm {

// =============================================================================
// LinuxPlatform 类声明
// =============================================================================

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

// =============================================================================
// 辅助函数
// =============================================================================

/// 读取 /proc/[pid]/stat 中的进程 jiffies
static bool readProcStat(pid_t pid, unsigned long long& utime,
                         unsigned long long& stime) {
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%d/stat", pid);

  FILE* fp = std::fopen(path, "r");
  if (!fp) return false;

  // 格式: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt
  //        majflt cmajflt utime stime cutime cstime ...
  unsigned long long val;
  int rc = std::fscanf(fp,
                       "%*d (%*[^)]) %*c %*d %*d %*d %*d %*d %*u %*u "
                       "%*u %*u %*u %llu %llu",
                       &utime, &stime);
  std::fclose(fp);

  return rc == 2;
}

/// 读取 /proc/stat 获取系统总 jiffies
static bool readSystemStat(unsigned long long& total_jiffies) {
  FILE* fp = std::fopen("/proc/stat", "r");
  if (!fp) return false;

  char buf[256];
  if (!std::fgets(buf, sizeof(buf), fp)) {
    std::fclose(fp);
    return false;
  }
  std::fclose(fp);

  // cpu user nice system idle iowait irq softirq steal
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
  int rc =
      std::sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user,
                  &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
  if (rc < 4) return false;

  total_jiffies = user + nice + system + idle + iowait + irq + softirq + steal;
  return true;
}

/// 读取 /proc/[pid]/status 中的 VmRSS
static uint64_t readVmRSS(pid_t pid) {
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%d/status", pid);

  FILE* fp = std::fopen(path, "r");
  if (!fp) return 0;

  char line[256];
  uint64_t rss = 0;
  while (std::fgets(line, sizeof(line), fp)) {
    if (std::sscanf(line, "VmRSS: %lu kB", &rss) == 1) {
      rss *= 1024;  // kB -> bytes
      break;
    }
  }
  std::fclose(fp);
  return rss;
}

// =============================================================================
// LinuxPlatform 实现
// =============================================================================

bool LinuxPlatform::processExists(pid_t pid) {
  if (pid <= 0) return false;
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%d", pid);
  return (access(path, F_OK) == 0);
}

pid_t LinuxPlatform::getProcessId(const std::string& process_name) {
  DIR* proc_dir = opendir("/proc");
  if (!proc_dir) return -1;

  struct dirent* entry;
  pid_t found_pid = -1;

  while ((entry = readdir(proc_dir)) != nullptr) {
    // 检查是否数字目录（PID）
    char* end;
    pid_t pid = std::strtol(entry->d_name, &end, 10);
    if (*end != '\0') continue;

    // 读取进程 cmdline 或 status 中的 Name
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    FILE* fp = std::fopen(path, "r");
    if (!fp) continue;

    char comm[256];
    if (std::fgets(comm, sizeof(comm), fp)) {
      // 去除换行符
      size_t len = std::strlen(comm);
      if (len > 0 && comm[len - 1] == '\n') comm[len - 1] = '\0';

      if (process_name == comm) {
        found_pid = pid;
        std::fclose(fp);
        break;
      }
    }
    std::fclose(fp);
  }

  closedir(proc_dir);
  return found_pid;
}

pid_t LinuxPlatform::startProcess(const std::string& path,
                                  const std::vector<std::string>& args) {
  pid_t pid = fork();
  if (pid == 0) {
    // 子进程
    // 构建 argv
    std::vector<const char*> argv;
    argv.push_back(path.c_str());
    for (const auto& arg : args) {
      argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    // 重定向 stdin/stdout/stderr 到 /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null >= 0) {
      dup2(dev_null, STDIN_FILENO);
      dup2(dev_null, STDOUT_FILENO);
      dup2(dev_null, STDERR_FILENO);
      close(dev_null);
    }

    execvp(path.c_str(), const_cast<char* const*>(argv.data()));

    // exec 失败
    _exit(127);
  }

  return pid;
}

bool LinuxPlatform::stopProcess(pid_t pid, int sig) {
  return (kill(pid, sig) == 0);
}

bool LinuxPlatform::waitProcess(pid_t pid, std::chrono::milliseconds timeout) {
  int status;
  pid_t result;

  if (timeout.count() <= 0) {
    result = waitpid(pid, &status, WNOHANG);
    return (result == pid) && WIFEXITED(status);
  }

  // 轮询等待
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      return WIFEXITED(status);
    }
    if (result == -1) {
      return false;  // 错误
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return false;  // 超时
}

double LinuxPlatform::getCpuUsage(pid_t pid) {
  // 两次采样计算 CPU 使用率
  static auto last_read = std::chrono::steady_clock::now();
  static unsigned long long last_total_jiffies = 0;
  static unsigned long long last_proc_jiffies = 0;

  unsigned long long utime, stime;
  if (!readProcStat(pid, utime, stime)) return 0.0;

  unsigned long long total_jiffies;
  if (!readSystemStat(total_jiffies)) return 0.0;

  auto now = std::chrono::steady_clock::now();

  if (last_total_jiffies == 0) {
    // 首次采样，记录基线
    last_proc_jiffies = utime + stime;
    last_total_jiffies = total_jiffies;
    last_read = now;
    return 0.0;
  }

  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_read)
          .count();
  if (elapsed <= 0) return 0.0;

  unsigned long long proc_delta = (utime + stime) - last_proc_jiffies;
  unsigned long long sys_delta = total_jiffies - last_total_jiffies;

  last_proc_jiffies = utime + stime;
  last_total_jiffies = total_jiffies;
  last_read = now;

  if (sys_delta == 0) return 0.0;

  // CPU 使用率 = 进程 jiffies / 系统总 jiffies * 100 * 核数
  double cpu_percent = (double)proc_delta / (double)sys_delta * 100.0;
  cpu_percent *= sysconf(_SC_NPROCESSORS_ONLN);

  return cpu_percent;
}

uint64_t LinuxPlatform::getMemoryUsageBytes(pid_t pid) {
  return readVmRSS(pid);
}

uint64_t LinuxPlatform::getTotalSystemMemory() {
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return info.totalram * info.mem_unit;
  }
  return 0;
}

uint32_t LinuxPlatform::getOpenFileCount(pid_t pid) {
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%d/fd", pid);

  DIR* fd_dir = opendir(path);
  if (!fd_dir) return 0;

  uint32_t count = 0;
  struct dirent* entry;
  while ((entry = readdir(fd_dir)) != nullptr) {
    if (entry->d_type == DT_LNK || entry->d_type == DT_UNKNOWN) {
      count++;
    }
  }
  closedir(fd_dir);

  // 减去 . 和 ..
  return count > 2 ? count - 2 : 0;
}

double LinuxPlatform::getSystemCpuLoad() {
  unsigned long long total_jiffies;
  if (!readSystemStat(total_jiffies)) return 0.0;

  static unsigned long long last_total = 0;
  static auto last_time = std::chrono::steady_clock::now();

  if (last_total == 0) {
    last_total = total_jiffies;
    last_time = std::chrono::steady_clock::now();
    return 0.0;
  }

  auto now = std::chrono::steady_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time)
          .count();

  if (elapsed_ms <= 0) return 0.0;

  double delta = static_cast<double>(total_jiffies - last_total);
  // 转换为百分比 (jiffies per ms * 1000 -> per second, / 100 -> percent)
  double load = delta / elapsed_ms * 100.0 / sysconf(_SC_NPROCESSORS_ONLN);

  last_total = total_jiffies;
  last_time = now;

  return std::min(load, 100.0);
}

std::string LinuxPlatform::getHostname() {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) == 0) {
    buf[sizeof(buf) - 1] = '\0';
    return buf;
  }
  return "unknown";
}

uint64_t LinuxPlatform::getSystemUptimeMs() {
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return static_cast<uint64_t>(info.uptime) * 1000;
  }
  return 0;
}

bool LinuxPlatform::checkThreadAlive(pthread_t thread,
                                     std::chrono::milliseconds timeout) {
  // 使用 pthread_timedjoin_np 进行零超时探测
  // 如果可以 join，说明线程已退出
  void* retval;
  int rc = pthread_timedjoin_np(thread, &retval,
                                nullptr);  // nullptr = 不等待
  if (rc == 0) {
    // 线程已退出
    return false;
  }
  return true;  // 线程仍在运行或 join 失败
}

}  // namespace phm
}  // namespace faw