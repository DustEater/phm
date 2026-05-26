#include "daemon.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "faw/phm/config_parser.h"
#include "faw/phm/logger.h"
#include "ipc_server.h"

namespace faw {
namespace phm {

Daemon* Daemon::s_instance = nullptr;

Daemon::Daemon(DaemonConfig config) : config_(std::move(config)) {}

Daemon::~Daemon() {
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

bool Daemon::initialize() {
  // 设置日志
  auto& logger = Logger::instance();
  if (!config_.phm.log_dir.empty()) {
    std::string log_path = config_.phm.log_dir + "/phmd.log";
    logger.setOutput(log_path);
  }
  logger.setLevel(Logger::INFO);

  PHM_LOG_INFO("PHM daemon v%s initializing...", config_.phm.version.c_str());

  // 注册信号处理
  s_instance = this;
  std::signal(SIGTERM, Daemon::signalHandler);
  std::signal(SIGINT, Daemon::signalHandler);
  std::signal(SIGHUP, Daemon::signalHandler);
  std::signal(SIGPIPE, SIG_IGN);

  // 创建引擎
  engine_ = std::make_unique<PhmEngine>(config_.phm);

  // 加载配置
  if (!config_.phm.config_path.empty()) {
    if (!engine_->loadConfiguration(config_.phm.config_path)) {
      PHM_LOG_WARN("Failed to load config from %s, using defaults",
                   config_.phm.config_path.c_str());
    }
  }

  PHM_LOG_INFO("PHM daemon initialized successfully");
  return true;
}

int Daemon::run() {
  if (!engine_) {
    PHM_LOG_ERROR("Engine not initialized");
    return 1;
  }

  // 守护进程化
  if (config_.daemonize) {
    if (!daemonize()) {
      return 1;
    }
  }

  // 写 PID 文件
  writePidFile();

  // 启动引擎
  if (!engine_->start()) {
    PHM_LOG_ERROR("Failed to start PHM engine");
    return 1;
  }

  // 启动 IPC 服务器
  IpcServer ipc_server(engine_.get(), config_.phm.ipc_endpoint);
  if (!ipc_server.start()) {
    PHM_LOG_WARN("Failed to start IPC server (non-fatal)");
  }

  running_ = true;
  PHM_LOG_INFO("PHM daemon running (pid=%d)", getpid());

  // 主循环
  while (running_) {
    // 处理 IPC 客户端连接
    if (ipc_server.isRunning()) {
      ipc_server.acceptOne();
    }

    // 处理配置重载请求
    if (reload_requested_) {
      reload_requested_ = false;
      PHM_LOG_INFO("Reloading configuration...");
      if (engine_->reloadConfiguration()) {
        PHM_LOG_INFO("Configuration reloaded successfully");
      } else {
        PHM_LOG_ERROR("Configuration reload failed");
      }
    }

    // 睡眠等待（使用条件变量可中断等待，此处简化用 sleep）
    sleep(1);
  }

  // 清理
  PHM_LOG_INFO("PHM daemon shutting down...");
  ipc_server.stop();
  engine_->stop();
  removePidFile();

  PHM_LOG_INFO("PHM daemon exited cleanly");
  return 0;
}

void Daemon::requestShutdown() { running_ = false; }

void Daemon::requestReload() { reload_requested_ = true; }

bool Daemon::isRunning() const noexcept { return running_; }

void Daemon::signalHandler(int sig) {
  if (!s_instance) return;

  switch (sig) {
    case SIGTERM:
    case SIGINT:
      PHM_LOG_INFO("Received signal %d, shutting down", sig);
      s_instance->requestShutdown();
      break;
    case SIGHUP:
      PHM_LOG_INFO("Received SIGHUP, reloading config");
      s_instance->requestReload();
      break;
  }
}

bool Daemon::daemonize() {
  pid_t pid = fork();
  if (pid < 0) {
    PHM_LOG_ERROR("fork() failed: %s", std::strerror(errno));
    return false;
  }

  if (pid > 0) {
    // 父进程退出
    _exit(0);
  }

  // 子进程继续
  if (setsid() < 0) {
    PHM_LOG_ERROR("setsid() failed: %s", std::strerror(errno));
    return false;
  }

  // 第二次 fork 防止获取终端
  pid = fork();
  if (pid < 0) {
    PHM_LOG_ERROR("Second fork() failed: %s", std::strerror(errno));
    return false;
  }
  if (pid > 0) {
    _exit(0);
  }

  // 设置文件权限掩码
  umask(0);

  // 切换到根目录
  if (chdir("/") < 0) {
    PHM_LOG_WARN("chdir('/') failed");
  }

  // 关闭所有文件描述符
  for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
    close(i);
  }

  // 重定向 stdin/stdout/stderr 到 /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd >= 0) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
  }

  return true;
}

bool Daemon::writePidFile() {
  if (config_.pid_file.empty()) return true;

  FILE* fp = fopen(config_.pid_file.c_str(), "w");
  if (!fp) {
    PHM_LOG_ERROR("Cannot write PID file %s: %s", config_.pid_file.c_str(),
                  std::strerror(errno));
    return false;
  }

  fprintf(fp, "%d\n", getpid());
  fclose(fp);

  // 限制权限
  chmod(config_.pid_file.c_str(), 0644);
  return true;
}

void Daemon::removePidFile() {
  if (!config_.pid_file.empty()) {
    unlink(config_.pid_file.c_str());
  }
}

}  // namespace phm
}  // namespace faw