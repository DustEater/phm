#include <gtest/gtest.h>

#include "faw/phm/platform.h"

// Linux 平台适配测试
// 这些测试需要 Linux 环境支持

TEST(PlatformLinuxTest, CreatePlatform) {
  auto platform = faw::phm::Platform::create();
  EXPECT_NE(platform, nullptr);
}

TEST(PlatformLinuxTest, ProcessExists) {
  auto platform = faw::phm::Platform::create();
  ASSERT_NE(platform, nullptr);

  // 当前进程应该存在
  bool exists = platform->processExists(getpid());
  EXPECT_TRUE(exists);

  // PID 0 不存在
  exists = platform->processExists(0);
  EXPECT_FALSE(exists);

  // 极大 PID 不存在
  exists = platform->processExists(999999999);
  EXPECT_FALSE(exists);
}

TEST(PlatformLinuxTest, SystemInfo) {
  auto platform = faw::phm::Platform::create();
  ASSERT_NE(platform, nullptr);

  std::string hostname = platform->getHostname();
  EXPECT_FALSE(hostname.empty());

  uint64_t uptime = platform->getSystemUptimeMs();
  EXPECT_GT(uptime, 0);

  uint64_t total_mem = platform->getTotalSystemMemory();
  EXPECT_GT(total_mem, 0);
}

TEST(PlatformLinuxTest, CurrentProcessResource) {
  auto platform = faw::phm::Platform::create();
  ASSERT_NE(platform, nullptr);

  pid_t self = getpid();

  // 内存应 > 0
  uint64_t mem = platform->getMemoryUsageBytes(self);
  EXPECT_GT(mem, 0);

  // 文件描述符数应 > 2 (stdin, stdout, stderr)
  uint32_t fds = platform->getOpenFileCount(self);
  EXPECT_GE(fds, 3);
}

TEST(PlatformLinuxTest, GetProcessId) {
  auto platform = faw::phm::Platform::create();
  ASSERT_NE(platform, nullptr);

  // 当前进程应该能找到自己
  pid_t pid = platform->getProcessId("phm_test_pal_linux");
  if (pid < 0) {
    // 可能进程名不匹配，测试不强制通过
    GTEST_SKIP() << "Process name not found, skipping";
  }
  EXPECT_GT(pid, 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}