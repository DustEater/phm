#include <gtest/gtest.h>

// 抖动抑制测试已在 test_state_machine.cpp 中覆盖
// 本文件包含附加值测试

#include "faw/phm/supervised_entity.h"

using namespace faw::phm;

TEST(DebounceTest, SingleShotImmediate) {
  // debounce_count = 1 时，一次故障应立即触发状态转换
  MonitorConfig mc;
  mc.type = MonitorType::CUSTOM;
  mc.debounce_count = 1;

  SEConfig cfg;
  cfg.name = "test_immediate";
  cfg.monitors.push_back(mc);
  SupervisedEntity se(cfg);

  PhmEvent evt;
  evt.source_monitor = "custom_1";
  evt.severity = Severity::ERROR;

  se.onMonitorEvent(evt);
  EXPECT_EQ(se.getState(), SeState::ERROR);
}

TEST(DebounceTest, RecoveryAfterSuspect) {
  // 在 SUSPECT 状态恢复正常
  MonitorConfig mc;
  mc.type = MonitorType::CUSTOM;
  mc.debounce_count = 3;

  SEConfig cfg;
  cfg.name = "test_recovery";
  cfg.monitors.push_back(mc);
  SupervisedEntity se(cfg);

  PhmEvent err_evt;
  err_evt.source_monitor = "custom_1";
  err_evt.severity = Severity::ERROR;

  // 第1次故障 -> SUSPECT
  se.onMonitorEvent(err_evt);
  EXPECT_EQ(se.getState(), SeState::SUSPECT);

  // 恢复正常事件
  PhmEvent ok_evt;
  ok_evt.source_monitor = "custom_1";
  ok_evt.severity = Severity::INFO;

  se.onMonitorEvent(ok_evt);
  EXPECT_EQ(se.getState(), SeState::RUNNING);
}

TEST(DebounceTest, LargeWindow) {
  // 大的抖动窗口
  MonitorConfig mc;
  mc.type = MonitorType::CUSTOM;
  mc.debounce_count = 10;

  SEConfig cfg;
  cfg.name = "test_large_window";
  cfg.monitors.push_back(mc);
  SupervisedEntity se(cfg);

  PhmEvent evt;
  evt.source_monitor = "custom_1";
  evt.severity = Severity::ERROR;

  // 前9次 -> SUSPECT
  for (int i = 0; i < 9; i++) {
    se.onMonitorEvent(evt);
  }
  EXPECT_EQ(se.getState(), SeState::SUSPECT);

  // 第10次 -> ERROR
  se.onMonitorEvent(evt);
  EXPECT_EQ(se.getState(), SeState::ERROR);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}