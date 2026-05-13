#ifndef FAW_PHM_PLATFORM_H
#define FAW_PHM_PLATFORM_H

/// @file platform.h
/// @brief 平台抽象层（PAL）接口定义

#include <pthread.h>
#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace faw {
namespace phm {

/// 平台抽象层（Platform Abstraction Layer）
///
/// 提供跨平台的进程管理、资源监控和系统信息接口。
/// Linux 和 QNX 各自实现此接口的具体适配器。
class Platform {
 public:
  virtual ~Platform() = default;

  // ===== 进程管理 =====

  /// 检查进程是否存在
  virtual bool processExists(pid_t pid) = 0;

  /// 通过进程名获取 PID
  /// @param process_name 进程名称
  /// @return PID，未找到返回 -1
  virtual pid_t getProcessId(const std::string& process_name) = 0;

  /// 启动进程
  /// @param path 可执行文件路径
  /// @param args 命令行参数
  /// @return 子进程 PID，失败返回 -1
  virtual pid_t startProcess(const std::string& path,
                             const std::vector<std::string>& args) = 0;

  /// 停止进程
  /// @param pid 进程 ID
  /// @param sig 信号编号（默认 SIGTERM=15）
  /// @return true 操作成功
  virtual bool stopProcess(pid_t pid, int sig = 15) = 0;

  /// 等待进程退出
  /// @param pid 进程 ID
  /// @param timeout 超时时间
  /// @return true 进程正常退出，false 超时或失败
  virtual bool waitProcess(pid_t pid, std::chrono::milliseconds timeout) = 0;

  // ===== 资源监控 =====

  /// 获取进程 CPU 使用率（百分比，相对于单核）
  /// @param pid 进程 ID
  /// @return CPU 使用率 (0.0 ~ 100.0 * 核数)
  virtual double getCpuUsage(pid_t pid) = 0;

  /// 获取进程物理内存占用（字节）
  virtual uint64_t getMemoryUsageBytes(pid_t pid) = 0;

  /// 获取系统总物理内存（字节）
  virtual uint64_t getTotalSystemMemory() = 0;

  /// 获取进程打开的文件描述符数量
  virtual uint32_t getOpenFileCount(pid_t pid) = 0;

  /// 获取系统整体 CPU 负载（百分比）
  virtual double getSystemCpuLoad() = 0;

  // ===== 系统信息 =====

  /// 获取主机名
  virtual std::string getHostname() = 0;

  /// 获取系统运行时间（毫秒）
  virtual uint64_t getSystemUptimeMs() = 0;

  // ===== 死锁/挂死检测 =====

  /// 检测线程是否存活
  /// @param thread pthread_t 句柄
  /// @param timeout 检测超时
  /// @return true 线程存活且在超时内响应
  virtual bool checkThreadAlive(pthread_t thread,
                                std::chrono::milliseconds timeout) = 0;

  // ===== 工厂 =====

  /// 创建平台实例（自动检测当前操作系统）
  static std::unique_ptr<Platform> create();
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_PLATFORM_H