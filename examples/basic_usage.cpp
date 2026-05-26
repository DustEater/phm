/// @file basic_usage.cpp
/// @brief PHM 供应商集成示例
///
/// 本示例展示供应商进程如何集成 PHM 健康管理能力：
///   1. 通过 HealthChannel 上报心跳
///   2. 通过 ProcessResourceReporter 自动采集并上报 CPU / 内存 / FD 指标
///
/// 供应商仅需包含 client.h 一个头文件，daemon 内部 API 物理不可见。
///
/// 编译:
///   g++ -std=c++17 -I../include -L../build/src basic_usage.cpp -lphm -lpthread -lrt -o supplier_example
/// 运行:
///   LD_LIBRARY_PATH=../build/src ./supplier_example

#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <thread>

#include "faw/phm/client.h"
#include "faw/phm/resource_reporter.h"

namespace {

volatile bool g_running = true;

void signalHandler(int) { g_running = false; }

}  // namespace

int main() {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // =========================================================================
  // 1. 创建 HealthChannel -- 供应商与 PHM daemon 的通信通道
  // =========================================================================
  // 通道名称建议与进程/模块名一致，daemon 侧通过同名 SE 配置关联。
  // 构造函数第 2 个参数为存活超时，daemon 在该时间内未收到心跳即判定异常。
  std::cout << "=== 创建 HealthChannel ===" << std::endl;
  faw::phm::HealthChannel hc("supplier_module",
                             std::chrono::milliseconds(5000));

  // =========================================================================
  // 2. 心跳上报 -- 供应商进程在主循环中递增计数器并上报
  // =========================================================================
  // report() 将计数器写入共享内存，daemon 侧 AliveMonitor 通过检查计数器
  // 是否持续递增来判断进程活性。
  std::cout << "=== 启动心跳上报 ===" << std::endl;
  uint64_t alive_counter = 0;

  // 可选：同时上报自定义状态（如版本号、配置哈希）
  hc.reportStatus("version", "1.0.0");

  // =========================================================================
  // 3. 资源自采 -- 使用 ProcessResourceReporter 自动上报 CPU/内存/FD
  // =========================================================================
  // ProcessResourceReporter 内部通过 getrusage(RUSAGE_SELF) 采集进程自身
  // 资源开销，定期写入 HealthChannel 共享内存指标区。
  // daemon 侧 ResourceMonitor 优先读取此数据，实现零开销 OS 兜底。
  std::cout << "=== 启动资源自采上报 ===" << std::endl;
  faw::phm::ProcessResourceReporter reporter(hc,
                                             std::chrono::milliseconds(2000));
  reporter.start();

  // =========================================================================
  // 4. 模拟业务主循环
  // =========================================================================
  std::cout << "=== 供应商进程运行中 (Ctrl+C 退出) ===" << std::endl;

  auto start_time = std::chrono::steady_clock::now();

  while (g_running) {
    // 业务逻辑 ...
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 心跳上报：递增计数器并写入 HealthChannel
    hc.report(++alive_counter);

    // 定期输出状态
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();
    if (elapsed % 5 == 0 && elapsed > 0) {
      std::cout << "[运行 " << elapsed << "s] alive_counter=" << alive_counter
                << std::endl;
    }
  }

  // =========================================================================
  // 5. 清理
  // =========================================================================
  std::cout << "=== 停止资源采集 ===" << std::endl;
  reporter.stop();

  std::cout << "=== 供应商进程退出 ===" << std::endl;
  return 0;
}