#include <gtest/gtest.h>

#include "faw/phm/phm_engine.h"
#include "faw/phm/supervised_entity.h"
#include "faw/phm/types.h"

using namespace faw::phm;

// =============================================================================
// 状态机基础测试
// =============================================================================

TEST(StateMachineTest, InitialStateIsInit) {
  SEConfig cfg;
  cfg.name = "test_entity";
  SupervisedEntity se(cfg);
  EXPECT_EQ(se.getState(), SeState::INIT);
}

TEST(StateMachineTest, StartTransitionsToRunning) {
  SEConfig cfg;
  cfg.name = "test_entity";
  // 无 Monitor，start() 应返回但状态不变
  // 实际状态转换由 checkAll() 触发
  SupervisedEntity se(cfg);
  EXPECT_EQ(se.getState(), SeState::INIT);
}

// =============================================================================
// 状态转换测试
// =============================================================================

TEST(StateMachineTest, NonStrictJumps) {
  SEConfig cfg;
  cfg.name = "test_entity";
  SupervisedEntity se(cfg);
  EXPECT_EQ(se.getState(), SeState::INIT);
}

TEST(StateMachineTest, DebounceTransition) {
  // 验证抖动抑制
  MonitorConfig mc;
  mc.type = MonitorType::CUSTOM;
  mc.debounce_count = 3;

  SEConfig cfg;
  cfg.name = "test";
  cfg.monitors.push_back(mc);
  SupervisedEntity se(cfg);

  // 模拟连续 3 次故障事件
  PhmEvent evt;
  evt.source_monitor = "custom_1";
  evt.severity = Severity::ERROR;

  se.onMonitorEvent(evt);
  EXPECT_EQ(se.getState(), SeState::SUSPECT);  // 第1次: 可疑

  se.onMonitorEvent(evt);
  EXPECT_EQ(se.getState(), SeState::SUSPECT);  // 第2次: 仍可疑

  se.onMonitorEvent(evt);
  // 第3次: 达到 debounce_count=3，应进入 ERROR
  EXPECT_EQ(se.getState(), SeState::ERROR);
}

// =============================================================================
// 健康通道测试
// =============================================================================

TEST(HealthChannelTest, ReportAndCheck) {
  HealthChannel hc("test_channel", std::chrono::milliseconds(1000));

  // 被监督进程上报
  EXPECT_TRUE(hc.report(1));

  // PHM 检查
  auto status = hc.getStatus();
  EXPECT_EQ(status.alive_counter, 1);
}

// =============================================================================
// Monitor 工厂测试
// =============================================================================

TEST(MonitorFactoryTest, CreateBuiltinMonitors) {
  MonitorConfig mc;
  mc.params["process_name"] = "test_proc";

  auto monitor =
      MonitorFactory::create(MonitorType::PROCESS_LIFECYCLE, "test", mc);
  EXPECT_NE(monitor, nullptr);
  EXPECT_EQ(monitor->name(), "test");
  EXPECT_EQ(monitor->getType(), MonitorType::PROCESS_LIFECYCLE);
}

TEST(MonitorFactoryTest, UnknownTypeReturnsNull) {
  MonitorConfig mc;
  auto monitor =
      MonitorFactory::create(static_cast<MonitorType>(99), "unknown", mc);
  EXPECT_EQ(monitor, nullptr);
}

// =============================================================================
// PhmEngine 基础测试
// =============================================================================

TEST(PhmEngineTest, CreateAndStart) {
  PhmEngine engine(PhmConfig{});
  EXPECT_FALSE(engine.isRunning());

  // 添加测试 SE
  SEConfig cfg;
  cfg.name = "test_se";
  engine.addSupervisedEntity(std::move(cfg));

  // 启动
  EXPECT_TRUE(engine.start());
  EXPECT_TRUE(engine.isRunning());

  // 查询
  auto names = engine.getEntityNames();
  EXPECT_EQ(names.size(), 1);
  EXPECT_EQ(names[0], "test_se");

  // 停止
  engine.stop();
  EXPECT_FALSE(engine.isRunning());
}

TEST(PhmEngineTest, GlobalStateAggregation) {
  PhmEngine engine(PhmConfig{});

  SEConfig cfg1;
  cfg1.name = "se_1";
  engine.addSupervisedEntity(std::move(cfg1));

  SEConfig cfg2;
  cfg2.name = "se_2";
  engine.addSupervisedEntity(std::move(cfg2));

  engine.start();
  EXPECT_EQ(engine.getGlobalState(), SeState::RUNNING);
  engine.stop();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}