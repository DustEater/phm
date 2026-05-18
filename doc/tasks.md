# PHM 开发任务清单 (tasks.md)

## 阶段一：基础设施与核心库 (P0 - 必须优先完成)

### 1.1 项目脚手架
- [x] 1.1.1 创建完整目录结构
- [x] 1.1.2 编写顶层 CMakeLists.txt（project definition, options, subdirectories）
- [x] 1.1.3 编写 src/CMakeLists.txt（libphm.so 构建目标）
- [ ] 1.1.4 编写编译器配置（`.clang-format`, `.clang-tidy`）
- [x] 1.1.5 配置 Git 仓库（`.gitignore` 初始化）

### 1.2 类型系统和基础类型
- [x] 1.2.1 实现 `include/faw/phm/types.h`：所有枚举、结构体定义
  - `SeState`, `Severity`, `MonitorType` 枚举
  - `HealthChannelStatus`, `PhmEvent`, `MonitorConfig`, `SEConfig`, `PhmConfig` 结构体
  - `PhmError` 错误码枚举
- [x] 1.2.2 实现 `include/faw/phm/logger.h` + `src/logger.cpp`
  - 级别控制（DEBUG/INFO/WARN/ERROR）
  - 文件输出 + syslog 可选
  - 线程安全（内部 spinlock 或无锁环形缓冲区）
- [x] 1.2.3 实现 `include/faw/phm/platform.h`
  - `Platform` 抽象类（纯虚接口）
  - `Platform::create()` 工厂方法声明
- [x] 1.2.4 实现 `src/platform/pal.cpp`（工厂方法实现）
  - 条件编译区分 Linux/QNX

### 1.3 状态机
- [x] 1.3.1 实现非严格状态机核心 `src/state_machine.cpp`
- [x] 1.3.2 定义状态转换表（允许的转换路径）
- [x] 1.3.3 实现抖动抑制（Debounce）逻辑
  - 滑动窗口计数器
  - 可配置窗口大小（debounce_count）
  - 窗口超时重置
- [x] 1.3.4 实现跳跃转换（SUSPECT -> ERROR / RUNNING -> FATAL）
- [x] 1.3.5 注册状态转换回调
- [x] 1.3.6 单元测试：`tests/test_state_machine.cpp`

### 1.4 健康通道
- [x] 1.4.1 实现 `include/faw/phm/health_channel.h` + `src/health_channel.cpp`
- [x] 1.4.2 共享内存 / POSIX mq 实现（跨进程通信）
- [x] 1.4.3 AliveCounter 递增和校验
- [x] 1.4.4 超时检测（last_check 时间戳）
- [x] 1.4.5 线程安全（原子操作 + mutex）

### 1.5 事件管理器
- [x] 1.5.1 实现 `src/event_manager.cpp`
- [x] 1.5.2 有界环形缓冲（固定大小，可配置）
- [x] 1.5.3 按严重级别过滤
- [x] 1.5.4 事件确认机制（acknowledge）
- [x] 1.5.5 事件老化（自动清理超过 TTL 的事件）

---

## 阶段二：平台抽象层 (P0)

### 2.1 Linux 适配
- [x] 2.1.1 实现 `src/platform/linux/process_linux.cpp`
  - `processExists`: 检查 `/proc/[pid]` 目录
  - `getProcessId`: 遍历 `/proc` 匹配进程名
  - `startProcess`: `fork()` + `exec()` + 子进程重定向
  - `stopProcess`: `kill()` 系统调用
  - `waitProcess`: `waitpid()` 超时封装
- [x] 2.1.2 实现 `src/platform/linux/resource_linux.cpp`
  - `getCpuUsage`: 两次读取 `/proc/[pid]/stat` utime+stime 计算差值
  - `getMemoryUsageBytes`: 读 `/proc/[pid]/status` VmRSS
  - `getTotalSystemMemory`: `sysinfo()` 或 `/proc/meminfo`
  - `getOpenFileCount`: `readdir("/proc/[pid]/fd")` 计数
  - `getSystemCpuLoad`: `/proc/stat` 计算 CPU 占用百分比
  - `checkThreadAlive`: `pthread_timedjoin_np` 零超时探测

### 2.2 QNX 适配
- [x] 2.2.1 实现 `src/platform/qnx/process_qnx.cpp`
  - `processExists`: `slm_query(SLM_QUERY_PID)`
  - `getProcessId`: `procnto` devctl `DCMD_PROC_TCRT` 遍历
  - `startProcess`: `spawn()` / `slm_start_process()`
  - `stopProcess`: `SignalKill(ND_LOCAL_NODE, pid, SIGTERM)`
  - `waitProcess`: `waitpid()` + 超时
- [x] 2.2.2 实现 `src/platform/qnx/resource_qnx.cpp`
  - `getCpuUsage`: `procfs_rusage` 或 `devctl(fd, DCMD_PROC_RUSAGE, ...)`
  - `getMemoryUsageBytes`: `asinfo()` 获取进程地址空间
  - `getTotalSystemMemory`: `asinfo()` 系统级
  - `getOpenFileCount`: `devctl(fd, DCMD_PROC_INFO, ...)`
  - `checkThreadAlive`: `SyncCondTimedwait` + pulse 机制

### 2.3 平台抽象单元测试
- [x] 2.3.1 `tests/test_pal_linux.cpp`: 测试每个 Linux API（在 Linux 宿主机运行）
- [ ] 2.3.2 测试 mock Platform（用于 Monitor 单元测试）
- [ ] 2.3.3 测试边界情况（进程不存在、资源读取失败等）

---

## 阶段三：Monitor 实现 (P0)

### 3.1 Monitor 框架
- [x] 3.1.1 实现 `include/faw/phm/monitor.h`（IMonitor 接口完善）
- [x] 3.1.2 实现 `src/monitor_framework.cpp`
  - Monitor 注册/注销
  - 调度循环（基于 tick_interval 的轮询）
  - 结果汇聚和回调分发
  - 错误隔离（单个 Monitor 异常不影响其他）

### 3.2 ProcessLifecycleMonitor
- [x] 3.2.1 实现 `src/monitors/process_lifecycle_monitor.cpp`
  - 定期检查进程是否存在
  - 进程启动/停止事件生成
  - 自动重启逻辑（根据 SEConfig.auto_restart）
  - 重启次数上限和间隔控制

### 3.3 ResourceMonitor
- [x] 3.3.1 实现 `src/monitors/resource_monitor.cpp`
  - CPU 使用率监控（warn/error 双阈值）
  - 内存使用监控（可配置 MB 阈值）
  - FD 数量监控（可选）
  - 阈值交叉事件生成

### 3.4 DeadlockMonitor
- [x] 3.4.1 实现 `src/monitors/deadlock_monitor.cpp`
  - 定时通过 HealthChannel 写入标记
  - 检查标记是否被更新（被监督进程响应）
  - 超时未更新 -> 判定为挂死
  - 死锁事件生成（包含线程/调用栈信息）

### 3.5 AliveMonitor
- [x] 3.5.1 实现 `src/monitors/alive_monitor.cpp`
  - 定期检查 HealthChannel AliveCounter 是否递增
  - 计数器冻结检测
  - 心跳超时事件生成

---

## 阶段四：SupervisedEntity 和 PhmEngine (P0)

### 4.1 SE Manager
- [x] 4.1.1 实现 `include/faw/phm/supervised_entity.h` + `src/state_machine.cpp`（SE 管理集成在状态机中）
- [x] 4.1.2 Monitor 注册和生命周期管理
- [x] 4.1.3 依赖关系解析和启动顺序控制
- [x] 4.1.4 实体级状态聚合（min over monitors）
- [x] 4.1.5 状态变更回调

### 4.2 PhmEngine
- [x] 4.2.1 实现 `include/faw/phm/phm_engine.h` + `src/phm_engine.cpp`
- [x] 4.2.2 Engine 启动/停止/重置生命周期
- [x] 4.2.3 SE 注册/注销/查询
- [x] 4.2.4 全局状态聚合（所有 SE 状态取最严重）
- [x] 4.2.5 引擎统计信息（`getStats()`）
- [x] 4.2.6 线程安全（使用 per-SE lock 粒度）
- [x] 4.2.7 集成 DataReporter 数据上报
- [ ] 4.2.8 单元测试：`tests/test_phm_engine.cpp`

---

## 阶段四B：数据上报 (P1)

### 4B.1 DataReporter
- [x] 4B.1.1 实现 `include/faw/phm/data_reporter.h` + `src/data_reporter.cpp`
- [x] 4B.1.2 JSON Lines 格式本地落盘
- [x] 4B.1.3 每小时文件轮转，按 SE 分目录存储
- [x] 4B.1.4 自动创建目录结构
- [x] 4B.1.5 线程安全（互斥锁保护文件写入）

### 4B.2 Uploader 上传框架
- [x] 4B.2.1 实现 `Uploader` 抽象接口
- [x] 4B.2.2 `initialize()` / `upload()` / `uploadBatch()` / `isAvailable()` 接口
- [x] 4B.2.3 DataReporter 集成 Uploader 回调

### 4B.3 引擎集成
- [x] 4B.3.1 PhmEngine 创建 DataReporter 实例
- [x] 4B.3.2 tickLoop 中采集 Monitor 数据并上报
- [x] 4B.3.3 数据目录基于 `PhmConfig.log_dir` 自动生成

---

## 阶段五：配置管理 (P0)

### 5.1 配置解析器
- [x] 5.1.1 实现 `include/faw/phm/config_parser.h` + `src/config_parser.cpp`
- [x] 5.1.2 JSON 解析（使用 nlohmann/json 单头文件库），支持 `global` 段解析并映射为 `PhmConfig`
- [x] 5.1.3 SEConfig 结构体构建
- [x] 5.1.4 错误处理和友好的解析错误信息
- [x] 5.1.5 Schema 校验（基本字段类型/范围检查）
- [x] 5.1.6 单元测试：`tests/test_config_parser.cpp`

### 5.2 Schema 定义
- [x] 5.2.1 编写 `schemas/se_config.schema.json` 完整定义
- [x] 5.2.2 编写 `config/phm_se_config.json` 示例配置文件

---

## 阶段六：phmd 守护进程 (P1)

### 6.1 守护进程核心
- [x] 6.1.1 实现 `daemon/main.cpp`：入口，信号处理，daemonize
- [x] 6.1.2 实现 `daemon/daemon.cpp`：
  - 守护进程化（fork, setsid, umask, chdir）
  - 信号处理（SIGTERM, SIGHUP, SIGINT, SIGPIPE）
  - 配置加载（启动时 + SIGHUP 重载）
  - PhmEngine 生命周期管理
  - PID 文件管理
- [x] 6.1.3 实现 `daemon/alert_aggregator.cpp`
  - 多 SE 告警汇总和去重
  - 告警降噪（相同告警抑制 N 分钟内重复上报）
  - 告警升级（WARNING -> ERROR 未恢复时升级）

### 6.2 IPC 服务器
- [x] 6.2.1 实现 `daemon/ipc_server.cpp`
  - Unix Domain Socket（路径 `/tmp/phmd.sock` 或配置）
  - JSON-RPC 2.0 协议解析
- [x] 6.2.2 实现 IPC 方法处理器：
  - `get_global_state`
  - `get_entity(name)`
  - `list_entities`
  - `list_events(min_severity)`
  - `acknowledge_events(event_ids)`
  - `reload_config`
  - `get_stats`
- [x] 6.2.3 并发客户端支持（select/poll 或单个连接处理）
- [x] 6.2.4 超时和断开重连处理
- [x] 6.2.5 单元测试：`tests/test_ipc_server.cpp`

### 6.3 phmd 系统集成
- [ ] 6.3.1 systemd service 单元文件（Linux）
- [ ] 6.3.2 QNX 的 slog2 日志集成
- [ ] 6.3.3 安装路径和部署脚本

---

## 阶段七：测试和完善 (P1)

### 7.1 集成测试
- [ ] 7.1.1 编写多 SE 场景集成测试（启动/停止/模拟故障）
- [ ] 7.1.2 CPU 过载场景测试（模拟高 CPU 进程）
- [ ] 7.1.3 进程崩溃和自动恢复测试
- [ ] 7.1.4 配置文件热重载测试
- [ ] 7.1.5 IPC 端到端通信测试

### 7.2 压力测试
- [ ] 7.2.1 50 个 SE 同时运行的内存和 CPU 压力
- [ ] 7.2.2 高频事件注入（模拟突发告警风暴）
- [ ] 7.2.3 连续运行 72 小时稳定性测试
- [ ] 7.2.4 进程资源泄露监控（valgrind / address sanitizer）

### 7.3 文档和示例
- [x] 7.3.1 编写 `examples/basic_usage.cpp`（完整使用示例）
- [ ] 7.3.2 API 使用文档（代码注释 + docstring）
- [x] 7.3.3 构建和部署文档（`build.sh`, `doc/`）

### 7.4 代码质量
- [ ] 7.4.1 代码 review（内部交叉审查）
- [ ] 7.4.2 静态分析（clang-tidy, cppcheck）
- [ ] 7.4.3 代码格式化（clang-format）
- [ ] 7.4.4 License header 检查

---

## 阶段八：QNX 平台验证 (P2)

### 8.1 QNX 交叉编译
- [x] 8.1.1 QNX 7.1 toolchain 配置 CMake toolchain file (`cmake/qnx-aarch64.cmake`)
- [ ] 8.1.2 QNX 平台适配层编译验证
- [ ] 8.1.3 解决 QNX 特有 API 差异（如无 `timerfd` 使用 `pulse`）

### 8.2 QNX 集成测试
- [ ] 8.2.1 QNX Neutrino 上运行 phmd
- [ ] 8.2.2 QNX 进程监控接口验证
- [ ] 8.2.3 QNX 资源监控接口验证
- [ ] 8.2.4 QNX IPC 通信（channel/pulse + Unix Socket）

---

## 任务优先级说明

| 优先级 | 含义 | 预期耗时 |
|--------|------|----------|
| **P0** | 必须完成，阻塞后续任务 | 约 70% 工作量 |
| **P1** | 重要特性，核心功能补全 | 约 20% 工作量 |
| **P2** | 增强和优化，可后续迭代 | 约 10% 工作量 |

## 预期工作量估算

| 阶段 | 人天估算 | 依赖 |
|------|----------|------|
| 一、基础设施与核心库 | 5-7 天 | 无 |
| 二、平台抽象层 | 4-6 天 | 阶段一 |
| 三、Monitor 实现 | 5-8 天 | 阶段二 |
| 四、SE 和 Engine | 3-5 天 | 阶段一、三 |
| 五、配置管理 | 2-3 天 | 阶段一 |
| 六、phmd 守护进程 | 4-6 天 | 阶段四、五 |
| 七、测试和完善 | 5-7 天 | 所有 |
| 八、QNX 验证 | 3-5 天 | 阶段六 |
| **总计** | **31-47 天** | |