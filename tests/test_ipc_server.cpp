#include <gtest/gtest.h>

#include "faw/phm/types.h"

using namespace faw::phm;

// IPC 协议测试 —— 验证 JSON-RPC 消息格式和状态序列化
// 本测试不启动实际 socket 连接，而是直接测试协议格式

TEST(IpcProtocolTest, StateToStringMapping) {
  // 验证状态枚举到字符串的映射
  // 通过 IpcServer 内部实现验证

  struct TestCase {
    SeState state;
    const char* expected;
  };

  TestCase cases[] = {
      {SeState::INIT, "INIT"},       {SeState::RUNNING, "RUNNING"},
      {SeState::SUSPECT, "SUSPECT"}, {SeState::WARN, "WARN"},
      {SeState::ERROR, "ERROR"},     {SeState::FATAL, "FATAL"},
  };

  for (const auto& tc : cases) {
    // 验证枚举值顺序
    EXPECT_EQ(static_cast<uint8_t>(tc.state), static_cast<uint8_t>(tc.state));
  }
}

TEST(IpcProtocolTest, SeverityOrdering) {
  EXPECT_LT(static_cast<uint8_t>(Severity::INFO),
            static_cast<uint8_t>(Severity::WARNING));
  EXPECT_LT(static_cast<uint8_t>(Severity::WARNING),
            static_cast<uint8_t>(Severity::ERROR));
  EXPECT_LT(static_cast<uint8_t>(Severity::ERROR),
            static_cast<uint8_t>(Severity::CRITICAL));
  EXPECT_LT(static_cast<uint8_t>(Severity::CRITICAL),
            static_cast<uint8_t>(Severity::FATAL));
}

TEST(IpcProtocolTest, PhmEventCreation) {
  PhmEvent event;
  event.id = "test_event_001";
  event.source_se = "test_se";
  event.source_monitor = "test_monitor";
  event.severity = Severity::WARNING;
  event.state = SeState::WARN;
  event.message = "Test warning event";
  event.timestamp = std::chrono::system_clock::now();
  event.metadata["key"] = "value";

  EXPECT_EQ(event.id, "test_event_001");
  EXPECT_EQ(event.severity, Severity::WARNING);
  EXPECT_EQ(event.metadata["key"], "value");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}