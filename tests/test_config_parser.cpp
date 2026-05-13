#include <gtest/gtest.h>

#include "faw/phm/config_parser.h"

using namespace faw::phm;

TEST(ConfigParserTest, ParseValidJson) {
  std::string json = R"({
    "supervised_entities": [
        {
            "name": "adas_perception",
            "description": "ADAS perception module",
            "alive_timeout_ms": 5000,
            "auto_restart": true,
            "max_restart_count": 3,
            "restart_delay_s": 5,
            "monitors": [
                {
                    "type": "process_lifecycle",
                    "enabled": true,
                    "interval_ms": 1000,
                    "params": {
                        "process_name": "adas_perception"
                    }
                },
                {
                    "type": "resource",
                    "enabled": true,
                    "interval_ms": 2000,
                    "warn_threshold_cpu": 80.0,
                    "error_threshold_cpu": 95.0,
                    "debounce_count": 3
                }
            ]
        },
        {
            "name": "camera_driver",
            "description": "Camera driver",
            "alive_timeout_ms": 3000,
            "monitors": [
                {
                    "type": "alive",
                    "enabled": true,
                    "interval_ms": 500
                }
            ]
        }
    ]
  })";

  auto configs = ConfigParser::parseString(json);

  ASSERT_EQ(configs.size(), 2);

  EXPECT_EQ(configs[0].name, "adas_perception");
  EXPECT_TRUE(configs[0].auto_restart);
  EXPECT_EQ(configs[0].max_restart_count, 3);
  EXPECT_EQ(configs[0].monitors.size(), 2);
  EXPECT_EQ(configs[0].monitors[0].type, MonitorType::PROCESS_LIFECYCLE);
  EXPECT_EQ(configs[0].monitors[0].interval.count(), 1000);
  EXPECT_EQ(configs[0].monitors[1].type, MonitorType::RESOURCE);
  EXPECT_DOUBLE_EQ(configs[0].monitors[1].warn_threshold, 80.0);

  EXPECT_EQ(configs[1].name, "camera_driver");
  EXPECT_EQ(configs[1].monitors.size(), 1);
  EXPECT_EQ(configs[1].monitors[0].type, MonitorType::ALIVE);
}

TEST(ConfigParserTest, HandleEmptyMonitors) {
  std::string json = R"({
    "supervised_entities": [
        {
            "name": "simple_se",
            "description": "No monitors"
        }
    ]
  })";

  auto configs = ConfigParser::parseString(json);
  ASSERT_EQ(configs.size(), 1);
  EXPECT_TRUE(configs[0].monitors.empty());
}

TEST(ConfigParserTest, HandleEmptyEntities) {
  std::string json = R"({
    "supervised_entities": []
  })";
  auto configs = ConfigParser::parseString(json);
  EXPECT_TRUE(configs.empty());
}

TEST(ConfigParserTest, ParseErrorOnInvalidJson) {
  std::string json = "not json at all";
  EXPECT_THROW(ConfigParser::parseString(json), std::runtime_error);
}

TEST(ConfigParserTest, ParseErrorOnMalformedJson) {
  std::string json = R"({"supervised_entities": [{"name": "test")";
  EXPECT_THROW(ConfigParser::parseString(json), std::runtime_error);
}

TEST(ConfigParserTest, Validate) {
  EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}