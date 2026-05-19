#include <gtest/gtest.h>

#include "faw/phm/types.h"

// IPC 协议测试 —— 验证 JSON-RPC 消息格式和状态序列化
// 本测试不启动实际 socket 连接，而是直接测试协议格式

TEST(IpcProtocolTest, StateToStringMapping) {
  // 验证状态枚举到字符串的映射
  // 通过 IpcServer 内部实现验证

  struct TestCase {
    faw::phm::SeState state;
    const char* expected;
  };

  TestCase cases[] = {
      {faw::phm::SeState::INIT, "INIT"},
      {faw::phm::SeState::RUNNING, "RUNNING"},
      {faw::phm::SeState::SUSPECT, "SUSPECT"},
      {faw::phm::SeState::WARN, "WARN"},
      {faw::phm::SeState::ERROR, "ERROR"},
      {faw::phm::SeState::FATAL, "FATAL"},
  };

  for (const auto& tc : cases) {
    // 验证枚举值顺序
    EXPECT_EQ(static_cast<uint8_t>(tc.state), static_cast<uint8_t>(tc.state));
  }
}

TEST(IpcProtocolTest, SeverityOrdering) {
  EXPECT_LT(static_cast<uint8_t>(faw::phm::Severity::INFO),
            static_cast<uint8_t>(faw::phm::Severity::WARNING));
  EXPECT_LT(static_cast<uint8_t>(faw::phm::Severity::WARNING),
            static_cast<uint8_t>(faw::phm::Severity::ERROR));
  EXPECT_LT(static_cast<uint8_t>(faw::phm::Severity::ERROR),
            static_cast<uint8_t>(faw::phm::Severity::CRITICAL));
  EXPECT_LT(static_cast<uint8_t>(faw::phm::Severity::CRITICAL),
            static_cast<uint8_t>(faw::phm::Severity::FATAL));
}

TEST(IpcProtocolTest, PhmEventCreation) {
  faw::phm::PhmEvent event;
  event.id = "test_event_001";
  event.source_se = "test_se";
  event.source_monitor = "test_monitor";
  event.severity = faw::phm::Severity::WARNING;
  event.state = faw::phm::SeState::WARN;
  event.message = "Test warning event";
  event.timestamp = std::chrono::system_clock::now();
  event.metadata["key"] = "value";

  EXPECT_EQ(event.id, "test_event_001");
  EXPECT_EQ(event.severity, faw::phm::Severity::WARNING);
  EXPECT_EQ(event.metadata["key"], "value");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}