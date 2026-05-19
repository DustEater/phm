#include <gtest/gtest.h>

#include "faw/phm/monitor.h"

TEST(MonitorFrameworkTest, FactoryRegistration) {
  // 内置 Monitor 应已自动注册
  auto types = faw::phm::MonitorFactory::registeredTypes();
  EXPECT_GT(types.size(), 0);

  bool has_process = false;
  for (auto t : types) {
    if (t == faw::phm::MonitorType::PROCESS_LIFECYCLE) has_process = true;
  }
  EXPECT_TRUE(has_process);
}

TEST(MonitorFrameworkTest, CreateAllTypes) {
  faw::phm::MonitorConfig mc;

  // 进程生命周期
  auto pm = faw::phm::MonitorFactory::create(faw::phm::MonitorType::PROCESS_LIFECYCLE, "pm", mc);
  EXPECT_NE(pm, nullptr);

  // 资源
  auto rm = faw::phm::MonitorFactory::create(faw::phm::MonitorType::RESOURCE, "rm", mc);
  EXPECT_NE(rm, nullptr);

  // 死锁
  auto dm = faw::phm::MonitorFactory::create(faw::phm::MonitorType::DEADLOCK, "dm", mc);
  EXPECT_NE(dm, nullptr);

  // Alive
  auto am = faw::phm::MonitorFactory::create(faw::phm::MonitorType::ALIVE, "am", mc);
  EXPECT_NE(am, nullptr);
}

TEST(MonitorFrameworkTest, CustomFactoryRegistration) {
  // 注册自定义 Monitor
  faw::phm::MonitorFactory::registerFactory(
      faw::phm::MonitorType::CUSTOM,
      [](const std::string& name,
         const faw::phm::MonitorConfig& cfg) -> std::unique_ptr<faw::phm::IMonitor> {
        return nullptr;  // 简化：返回空
      });

  auto types = faw::phm::MonitorFactory::registeredTypes();
  bool has_custom = false;
  for (auto t : types) {
    if (t == faw::phm::MonitorType::CUSTOM) has_custom = true;
  }
  EXPECT_TRUE(has_custom);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}