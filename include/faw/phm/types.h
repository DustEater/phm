#ifndef FAW_PHM_TYPES_H
#define FAW_PHM_TYPES_H

/**
 * @file types.h
 * @brief PHM 核心类型定义：枚举、结构体、错误码
 * */

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace faw {
namespace phm {

// =============================================================================
// 枚举定义
// =============================================================================

/**
 * 监督实体状态（非严格状态机）
 * */
enum class SeState : uint8_t {
  INIT = 0,     ///< 初始状态，Monitor 尚未启动
  RUNNING = 1,  ///< 健康，所有指标正常
  SUSPECT = 2,  ///< 可疑，单次超阈值但未持续确认
  WARN = 3,     ///< 警告，持续超阈值但可恢复
  ERROR = 4,    ///< 错误，严重超阈值，功能可能受影响
  FATAL = 5     ///< 致命，进程已死或不可恢复
};

/**
 * 告警严重级别
 * */
enum class Severity : uint8_t {
  INFO = 0,      ///< 信息性事件
  WARNING = 1,   ///< 警告，需关注
  ERROR = 2,     ///< 错误，需处理
  CRITICAL = 3,  ///< 严重错误，需立即处理
  FATAL = 4      ///< 致命事件，系统不可用
};

/**
 * 监控类型
 * */
enum class MonitorType : uint8_t {
  PROCESS_LIFECYCLE = 0,  ///< 进程生命周期监控
  RESOURCE = 1,           ///< CPU/内存资源监控
  DEADLOCK = 2,           ///< 死锁/挂死检测
  ALIVE = 3,              ///< 心跳活性监控
  CUSTOM = 4              ///< 自定义监控
};

/**
 * PHM 错误码
 * */
enum class PhmError : int32_t {
  OK = 0,                    ///< 成功
  GENERIC_ERROR = -1,        ///< 通用错误
  NOT_INITIALIZED = -2,      ///< 引擎未初始化
  ALREADY_STARTED = -3,      ///< 引擎已启动
  ENTITY_NOT_FOUND = -4,     ///< 实体未找到
  ENTITY_EXISTS = -5,        ///< 实体已存在
  MONITOR_NOT_FOUND = -6,    ///< Monitor 未找到
  CONFIG_PARSE_ERROR = -7,   ///< 配置解析错误
  CONFIG_VALIDATE_ERR = -8,  ///< 配置校验错误
  IPC_ERROR = -9,            ///< IPC 通信错误
  PLATFORM_ERROR = -10,      ///< 平台 API 错误
  RESOURCE_EXHAUSTED = -11,  ///< 资源耗尽
  TIMEOUT = -12              ///< 操作超时
};

// =============================================================================
// 结构体定义
// =============================================================================

/**
 * 健康通道状态
 * */
struct HealthChannelStatus {
  uint64_t alive_counter{0};                         ///< Alive 计数器值
  bool is_alive{false};                              ///< 是否存活
  std::chrono::steady_clock::time_point last_check;  ///< 上次检查时间
};

/**
 * PHM 事件
 * */
struct PhmEvent {
  std::string id;                                   ///< 事件唯一 ID (UUID)
  std::string source_se;                            ///< 来源 SE 名称
  std::string source_monitor;                       ///< 来源 Monitor 名称
  Severity severity{Severity::INFO};                ///< 严重级别
  SeState state{SeState::INIT};                     ///< 关联状态
  std::string message;                              ///< 事件描述
  std::chrono::system_clock::time_point timestamp;  ///< 事件时间戳
  std::map<std::string, std::string> metadata;      ///< 扩展元数据
};

/**
 * 监控配置
 * */
struct MonitorConfig {
  MonitorType type{MonitorType::CUSTOM};      ///< 监控类型
  std::map<std::string, std::string> params;  ///< 监控特有参数
  std::chrono::milliseconds interval{1000};   ///< 检测周期 (ms)
  std::chrono::milliseconds timeout{5000};    ///< 检测超时 (ms)
  uint32_t debounce_count{3};                 ///< 抖动抑制连续次数
  double warn_threshold{0.0};                 ///< 警告阈值
  double error_threshold{0.0};                ///< 错误阈值
  bool enabled{true};                         ///< 是否启用
};

/**
 * 监督实体 SE 配置
 * */
struct SEConfig {
  std::string name;                               ///< SE 名称（唯一标识）
  std::string description;                        ///< 描述
  std::vector<MonitorConfig> monitors;            ///< Monitor 配置列表
  std::vector<std::string> dependencies;          ///< 依赖的 SE 名称列表
  std::chrono::milliseconds alive_timeout{5000};  ///< 存活超时 (ms)
  bool auto_restart{false};                       ///< 是否自动重启
  uint32_t max_restart_count{3};                  ///< 最大重启次数
  std::chrono::seconds restart_delay{5};          ///< 重启延迟 (s)
  bool enabled{true};                             ///< 是否启用
};

/**
 * 全局 PHM 配置
 * */
struct PhmConfig {
  std::string app_name{"phmd"};                       ///< 应用名称
  std::string version{"1.0.0"};                       ///< 版本号
  std::chrono::milliseconds tick_interval{100};       ///< 引擎心跳周期 (ms)
  std::string log_dir{"/var/log/phmd"};               ///< 日志目录
  std::string ipc_endpoint{"/tmp/phmd.sock"};         ///< IPC 端点路径
  std::string config_path{"/etc/phm/se_config.json"};  ///< 配置文件路径
  bool enable_auto_recovery{true};                    ///< 是否启用自动恢复
  uint32_t event_queue_size{1024};                    ///< 事件队列容量
};

/**
 * PhmEngine 统计信息
 * */
struct EngineStats {
  size_t total_entities{0};             ///< SE 总数
  size_t active_entities{0};            ///< 活跃 SE 数
  size_t total_monitors{0};             ///< Monitor 总数
  size_t pending_events{0};             ///< 待处理事件数
  SeState global_state{SeState::INIT};  ///< 全局状态
  uint64_t uptime_ms{0};                ///< 运行时间 (ms)
};

/**
 * 守护进程配置（扩展自 PhmConfig）
 * */
struct DaemonConfig {
  PhmConfig phm;                              ///< 基础 PHM 配置
  bool daemonize{true};                       ///< 是否守护进程化
  std::string pid_file{"/var/run/phmd.pid"};  ///< PID 文件路径
  std::string user{""};                       ///< 运行用户（空 = 当前用户）
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_TYPES_H