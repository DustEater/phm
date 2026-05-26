/**
 * @file ipc_server.h
 * @brief IPC 服务器 —— Unix Domain Socket JSON-RPC 接口
 * */

#ifndef FAW_PHM_IPC_SERVER_H
#define FAW_PHM_IPC_SERVER_H

#include <memory>
#include <string>

#include "faw/phm/phm_engine.h"

namespace faw {
namespace phm {

/**
 * IPC 服务器
 *
 * 通过 Unix Domain Socket 提供 JSON-RPC 2.0 接口。
 * 支持以下方法：
 *   - get_global_state
 *   - get_entity(name)
 *   - list_entities
 *   - list_events(min_severity)
 *   - acknowledge_events(event_ids)
 *   - reload_config
 *   - get_stats
 * */
class IpcServer {
 public:
  IpcServer(PhmEngine* engine, const std::string& endpoint);
  ~IpcServer();

  /**
   * 启动服务器
   * */
  bool start();

  /**
   * 停止服务器
   * */
  void stop();

  /**
   * 是否在运行
   * */
  bool isRunning() const noexcept;

  /**
   * 非阻塞接收并处理一个客户端连接
   * */
  void acceptOne();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_IPC_SERVER_H