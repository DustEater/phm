/// @file basic_usage.cpp
/// @brief PHM 库基本使用示例
///
/// 编译: g++ -std=c++17 -I../include -L../build/lib basic_usage.cpp -lphm
/// -lpthread -o phm_example 运行: LD_LIBRARY_PATH=../build/lib ./phm_example

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "faw/phm/phm.h"

int main() {
  // 1. 创建 PHM 引擎
  std::cout << "=== PHM 引擎初始化 ===" << std::endl;
  faw::phm::PhmConfig config;
  config.app_name = "phm_example";
  config.tick_interval = std::chrono::milliseconds(500);
  config.log_dir = "";  // 输出到 stderr

  faw::phm::PhmEngine engine(config);

  // 2. 配置 SE
  std::cout << "=== 配置监督实体 ===" << std::endl;

  // 2a. 通过代码直接添加 SE
  faw::phm::SEConfig se_cfg;
  se_cfg.name = "example_process";
  se_cfg.description = "示例监控进程";
  se_cfg.auto_restart = false;
  se_cfg.alive_timeout = std::chrono::milliseconds(3000);

  // 进程生命周期监控配置
  faw::phm::MonitorConfig proc_mc;
  proc_mc.type = faw::phm::MonitorType::PROCESS_LIFECYCLE;
  proc_mc.interval = std::chrono::milliseconds(1000);
  proc_mc.params["process_name"] = "nonexistent_process";  // 假设进程不存在
  se_cfg.monitors.push_back(proc_mc);

  // 资源监控配置
  faw::phm::MonitorConfig res_mc;
  res_mc.type = faw::phm::MonitorType::RESOURCE;
  res_mc.interval = std::chrono::milliseconds(2000);
  res_mc.warn_threshold = 80.0;   // CPU > 80% 告警
  res_mc.error_threshold = 95.0;  // CPU > 95% 错误
  res_mc.debounce_count = 3;
  se_cfg.monitors.push_back(res_mc);

  engine.addSupervisedEntity(std::move(se_cfg));

  // 2b. 也可以通过 XML 配置加载（如有配置文件）
  // engine.loadConfiguration("/etc/phm/se_config.xml");

  // 3. 设置全局状态变更回调
  engine.setGlobalStateCallback([](faw::phm::SeState old_state, faw::phm::SeState new_state) {
    std::cout << "[Global State] " << static_cast<int>(old_state) << " -> "
              << static_cast<int>(new_state) << std::endl;
  });

  // 4. 启动引擎
  std::cout << "=== 启动 PHM 引擎 ===" << std::endl;
  if (!engine.start()) {
    std::cerr << "引擎启动失败!" << std::endl;
    return 1;
  }
  std::cout << "引擎运行中..." << std::endl;

  // 5. 运行一段时间，查询状态
  for (int i = 0; i < 10; i++) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto stats = engine.getStats();
    std::cout << "--- Tick " << (i + 1) << " ---" << std::endl;
    std::cout << "  Global State: " << static_cast<int>(stats.global_state)
              << std::endl;
    std::cout << "  Entities: " << stats.total_entities
              << " (active: " << stats.active_entities << ")" << std::endl;
    std::cout << "  Monitors: " << stats.total_monitors << std::endl;
    std::cout << "  Uptime: " << stats.uptime_ms << "ms" << std::endl;

    // 查询特定 SE
    auto entity = engine.getEntity("example_process");
    if (entity) {
      std::cout << "  Entity '" << entity->name()
                << "' state: " << static_cast<int>(entity->getState())
                << std::endl;
    }
  }

  // 6. 停止引擎
  std::cout << "=== 停止 PHM 引擎 ===" << std::endl;
  engine.stop();
  std::cout << "示例运行完毕。" << std::endl;

  return 0;
}