#include "ipc_server.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <string>

#include "faw/phm/logger.h"
#include "faw/phm/types.h"

namespace faw {
namespace phm {

/// JSON-RPC 简单实现（无外部依赖）
/// 生产环境建议使用成熟的 JSON 库

class IpcServer::Impl {
 public:
  Impl(PhmEngine* engine, const std::string& endpoint)
      : engine_(engine), endpoint_(endpoint), server_fd_(-1), running_(false) {}

  ~Impl() { stop(); }

  bool start() {
    if (running_) return false;

    // 创建 Unix Domain Socket
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
      PHM_LOG_ERROR("Cannot create socket: %s", std::strerror(errno));
      return false;
    }

    // 移除已存在的 socket 文件
    unlink(endpoint_.c_str());

    // 绑定地址
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      PHM_LOG_ERROR("Cannot bind socket %s: %s", endpoint_.c_str(),
                    std::strerror(errno));
      close(server_fd_);
      server_fd_ = -1;
      return false;
    }

    // 监听
    if (listen(server_fd_, 5) < 0) {
      PHM_LOG_ERROR("Cannot listen on socket: %s", std::strerror(errno));
      close(server_fd_);
      server_fd_ = -1;
      return false;
    }

    // 设置权限
    chmod(endpoint_.c_str(), 0666);

    // 设置为非阻塞模式
    int flags = fcntl(server_fd_, F_GETFL, 0);
    if (flags >= 0) {
      fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
    }

    running_ = true;
    PHM_LOG_INFO("IPC server started at %s", endpoint_.c_str());
    return true;
  }

  void stop() {
    running_ = false;
    if (server_fd_ >= 0) {
      close(server_fd_);
      server_fd_ = -1;
      unlink(endpoint_.c_str());
    }
  }

  bool isRunning() const noexcept { return running_; }

  /// 处理一个客户端请求（非阻塞）
  void handleClient() {
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return;
      return;
    }

    // 读取请求
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
      close(client_fd);
      return;
    }
    buf[n] = '\0';

    // 简单解析 method
    std::string request(buf);
    std::string response = dispatch(request);

    // 发送响应
    write(client_fd, response.c_str(), response.size());
    close(client_fd);
  }

 private:
  std::string dispatch(const std::string& request) {
    std::string method;
    std::string id = "null";

    // 提取 id
    auto id_pos = request.find("\"id\"");
    if (id_pos != std::string::npos) {
      auto colon = request.find(':', id_pos + 4);
      if (colon != std::string::npos) {
        auto end = request.find_first_of(",}", colon + 1);
        if (end != std::string::npos) {
          id = request.substr(colon + 1, end - colon - 1);
          id.erase(0, id.find_first_not_of(" \t"));
          id.erase(id.find_last_not_of(" \t") + 1);
        }
      }
    }

    // 提取 method
    auto meth_pos = request.find("\"method\"");
    if (meth_pos == std::string::npos) {
      return makeError(id, -32600, "Invalid Request: missing method");
    }

    auto colon = request.find(':', meth_pos + 8);
    auto meth_start = request.find('"', colon + 1);
    if (meth_start == std::string::npos) {
      return makeError(id, -32600, "Invalid Request");
    }
    meth_start++;
    auto meth_end = request.find('"', meth_start);
    method = request.substr(meth_start, meth_end - meth_start);

    // 路由
    if (method == "get_global_state") {
      return handleGetGlobalState(id);
    } else if (method == "get_entity") {
      return handleGetEntity(id, request);
    } else if (method == "list_entities") {
      return handleListEntities(id);
    } else if (method == "list_events") {
      return handleListEvents(id, request);
    } else if (method == "reload_config") {
      return handleReloadConfig(id);
    } else if (method == "get_stats") {
      return handleGetStats(id);
    } else {
      return makeError(id, -32601, "Method not found: " + method);
    }
  }

  std::string handleGetGlobalState(const std::string& id) {
    SeState state = engine_->getGlobalState();
    return makeResult(id,
                      "{\"global_state\":\"" + stateToString(state) + "\"}");
  }

  std::string handleGetEntity(const std::string& id,
                              const std::string& request) {
    // 提取 params.name
    auto name_pos = request.find("\"name\"");
    if (name_pos == std::string::npos) {
      return makeError(id, -32602, "Missing param: name");
    }
    auto colon = request.find(':', name_pos + 6);
    auto val_start = request.find('"', colon + 1);
    if (val_start == std::string::npos) {
      return makeError(id, -32602, "Invalid param: name");
    }
    val_start++;
    auto val_end = request.find('"', val_start);
    std::string name = request.substr(val_start, val_end - val_start);

    auto entity = engine_->getEntity(name);
    if (!entity) {
      return makeError(id, -32602, "Entity not found: " + name);
    }

    std::string result = "{";
    result += "\"name\":\"" + entity->name() + "\",";
    result += "\"state\":\"" + stateToString(entity->getState()) + "\"";
    result += "}";
    return makeResult(id, result);
  }

  std::string handleListEntities(const std::string& id) {
    auto names = engine_->getEntityNames();
    std::string result = "[";
    for (size_t i = 0; i < names.size(); i++) {
      if (i > 0) result += ",";
      result += "\"" + names[i] + "\"";
    }
    result += "]";
    return makeResult(id, result);
  }

  std::string handleListEvents(const std::string& id,
                               const std::string& request) {
    // 简化：返回空事件列表
    return makeResult(id, "[]");
  }

  std::string handleReloadConfig(const std::string& id) {
    bool ok = engine_->reloadConfiguration();
    return makeResult(id,
                      ok ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}");
  }

  std::string handleGetStats(const std::string& id) {
    auto stats = engine_->getStats();
    std::string result = "{";
    result +=
        "\"total_entities\":" + std::to_string(stats.total_entities) + ",";
    result +=
        "\"active_entities\":" + std::to_string(stats.active_entities) + ",";
    result +=
        "\"total_monitors\":" + std::to_string(stats.total_monitors) + ",";
    result +=
        "\"pending_events\":" + std::to_string(stats.pending_events) + ",";
    result += "\"global_state\":\"" + stateToString(stats.global_state) + "\",";
    result += "\"uptime_ms\":" + std::to_string(stats.uptime_ms);
    result += "}";
    return makeResult(id, result);
  }

  static std::string makeResult(const std::string& id,
                                const std::string& result) {
    return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + result +
           "}\n";
  }

  static std::string makeError(const std::string& id, int code,
                               const std::string& msg) {
    return "{\"jsonrpc\":\"2.0\",\"id\":" + id +
           ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":\"" +
           msg + "\"}}\n";
  }

  static std::string stateToString(SeState s) {
    switch (s) {
      case SeState::INIT:
        return "INIT";
      case SeState::RUNNING:
        return "RUNNING";
      case SeState::SUSPECT:
        return "SUSPECT";
      case SeState::WARN:
        return "WARN";
      case SeState::ERROR:
        return "ERROR";
      case SeState::FATAL:
        return "FATAL";
      default:
        return "UNKNOWN";
    }
  }

  PhmEngine* engine_;
  std::string endpoint_;
  int server_fd_;
  volatile bool running_;
};

// =============================================================================
// IpcServer 公共接口
// =============================================================================

IpcServer::IpcServer(PhmEngine* engine, const std::string& endpoint)
    : impl_(std::make_unique<Impl>(engine, endpoint)) {}

IpcServer::~IpcServer() = default;

bool IpcServer::start() { return impl_->start(); }
void IpcServer::stop() { impl_->stop(); }
bool IpcServer::isRunning() const noexcept { return impl_->isRunning(); }
void IpcServer::acceptOne() { impl_->handleClient(); }

}  // namespace phm
}  // namespace faw