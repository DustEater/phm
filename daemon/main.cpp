/// @file main.cpp
/// @brief phmd 守护进程入口

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "daemon.h"

static void printUsage(const char* prog) {
  std::fprintf(stderr,
               "Usage: %s [options]\n"
               "Options:\n"
               "  -c <path>   Configuration JSON file path\n"
               "  -l <dir>    Log directory\n"
               "  -p <path>   PID file path\n"
               "  -s <path>   IPC socket path\n"
               "  -v          Verbose (debug log level)\n"
               "  -f          Foreground mode (do not daemonize)\n"
               "  -h          Show this help\n",
               prog);
}

int main(int argc, char* argv[]) {
  faw::phm::DaemonConfig config;

  // 解析参数
  int opt;
  while ((opt = getopt(argc, argv, "c:l:p:s:vfh")) != -1) {
    switch (opt) {
      case 'c':
        config.phm.config_path = optarg;
        break;
      case 'l':
        config.phm.log_dir = optarg;
        break;
      case 'p':
        config.pid_file = optarg;
        break;
      case 's':
        config.phm.ipc_endpoint = optarg;
        break;
      case 'v':
        config.phm.log_dir.clear();
        break;  // 输出到 stderr
      case 'f':
        config.daemonize = false;
        break;
      case 'h':
        printUsage(argv[0]);
        return 0;
      default:
        printUsage(argv[0]);
        return 1;
    }
  }

  faw::phm::Daemon daemon(std::move(config));

  if (!daemon.initialize()) {
    std::fprintf(stderr, "Failed to initialize daemon\n");
    return 1;
  }

  return daemon.run();
}