#include <gtest/gtest.h>

#include "faw/phm/monitor.h"

using namespace faw::phm;

TEST(MonitorFrameworkTest, FactoryRegistration) {
  // 内置 Monitor 应已自动注册
  auto types = MonitorFactory::registeredTypes();
  EXPECT_GT(types.size(), 0);

  bool has_process = false;
  for (auto t : types) {
    if (t == MonitorType::PROCESS_LIFECYCLE) has_process = true;
  }
  EXPECT_TRUE(has_process);
}

TEST(MonitorFrameworkTest, CreateAllTypes) {
  MonitorConfig mc;

  // 进程生命周期
  auto pm = MonitorFactory::create(MonitorType::PROCESS_LIFECYCLE, "pm", mc);
  EXPECT_NE(pm, nullptr);

  // 资源
  auto rm = MonitorFactory::create(MonitorType::RESOURCE, "rm", mc);
  EXPECT_NE(rm, nullptr);

  // 死锁
  auto dm = MonitorFactory::create(MonitorType::DEADLOCK, "dm", mc);
  EXPECT_NE(dm, nullptr);

  // Alive
  auto am = MonitorFactory::create(MonitorType::ALIVE, "am", mc);
  EXPECT_NE(am, nullptr);
}

TEST(MonitorFrameworkTest, CustomFactoryRegistration) {
  // 注册自定义 Monitor
  MonitorFactory::registerFactory(
      MonitorType::CUSTOM,
      [](const std::string& name,
         const MonitorConfig& cfg) -> std::unique_ptr<IMonitor> {
        return nullptr;  // 简化：返回空
      });

  auto types = MonitorFactory::registeredTypes();
  bool has_custom = false;
  for (auto t : types) {
    if (t == MonitorType::CUSTOM) has_custom = true;
  }
  EXPECT_TRUE(has_custom);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}