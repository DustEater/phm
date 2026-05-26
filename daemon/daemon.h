/**
 * @file daemon.h
 * @brief phmd 守护进程核心
 * */

#ifndef FAW_PHM_DAEMON_H
#define FAW_PHM_DAEMON_H

#include <memory>
#include <string>

#include "faw/phm/phm_engine.h"

namespace faw {
namespace phm {

/**
 * 守护进程核心
 *
 * 负责守护进程化、信号处理、配置文件加载等。
 * */
class Daemon {
 public:
  explicit Daemon(DaemonConfig config);
  ~Daemon();

  /**
   * 初始化（解析配置、创建引擎）
   * */
  bool initialize();

  /**
   * 运行（进入事件循环）
   * */
  int run();

  /**
   * 请求安全停止
   * */
  void requestShutdown();

  /**
   * 请求配置重载
   * */
  void requestReload();

  /**
   * 是否正在运行
   * */
  bool isRunning() const noexcept;

 private:
  /**
   * 信号处理
   * */
  static void signalHandler(int sig);
  static Daemon* s_instance;

  /**
   * 守护进程化
   * */
  bool daemonize();

  /**
   * 写 PID 文件
   * */
  bool writePidFile();

  /**
   * 清理 PID 文件
   * */
  void removePidFile();

  DaemonConfig config_;
  std::unique_ptr<PhmEngine> engine_;
  volatile bool running_{false};
  volatile bool reload_requested_{false};
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_DAEMON_H