#include "faw/phm/config_parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace faw {
namespace phm {

thread_local std::string ConfigParser::s_last_error;

// =============================================================================
// 内部辅助函数
// =============================================================================

namespace {

MonitorType monitorTypeFromString(const std::string& s) {
  if (s == "process_lifecycle") return MonitorType::PROCESS_LIFECYCLE;
  if (s == "resource") return MonitorType::RESOURCE;
  if (s == "deadlock") return MonitorType::DEADLOCK;
  if (s == "alive") return MonitorType::ALIVE;
  if (s == "custom") return MonitorType::CUSTOM;
  return MonitorType::CUSTOM;
}

MonitorConfig parseMonitorConfig(const json& j) {
  MonitorConfig mc;

  std::string type_str = j.value("type", "");
  mc.type = monitorTypeFromString(type_str);
  if (mc.type == MonitorType::CUSTOM && type_str != "custom") {
    throw std::runtime_error("Unknown monitor type: " + type_str);
  }

  mc.enabled = j.value("enabled", true);
  mc.interval = std::chrono::milliseconds(j.value("interval_ms", 1000U));
  mc.timeout = std::chrono::milliseconds(j.value("timeout_ms", 5000U));
  mc.debounce_count = j.value("debounce_count", 3U);
  mc.warn_threshold = j.value("warn_threshold", 0.0);
  mc.error_threshold = j.value("error_threshold", 0.0);

  double warn_cpu = j.value("warn_threshold_cpu", 0.0);
  if (warn_cpu > 0.0) mc.warn_threshold = warn_cpu;
  double err_cpu = j.value("error_threshold_cpu", 0.0);
  if (err_cpu > 0.0) mc.error_threshold = err_cpu;

  auto params = j.value("params", json::object());
  for (auto it = params.begin(); it != params.end(); ++it) {
    mc.params[it.key()] = it.value().get<std::string>();
  }

  return mc;
}

SEConfig parseEntityConfig(const json& j) {
  SEConfig cfg;

  cfg.name = j.value("name", "");
  if (cfg.name.empty()) {
    throw std::runtime_error("Entity missing required field: name");
  }

  cfg.description = j.value("description", "");
  cfg.alive_timeout = std::chrono::milliseconds(j.value("alive_timeout_ms", 5000));
  cfg.auto_restart = j.value("auto_restart", false);
  cfg.max_restart_count = j.value("max_restart_count", 3U);
  cfg.restart_delay = std::chrono::seconds(j.value("restart_delay_s", 5U));
  cfg.enabled = j.value("enabled", true);

  auto deps = j.value("dependencies", json::array());
  for (const auto& dep : deps) {
    cfg.dependencies.push_back(dep.get<std::string>());
  }

  auto monitors = j.value("monitors", json::array());
  for (const auto& m : monitors) {
    cfg.monitors.push_back(parseMonitorConfig(m));
  }

  return cfg;
}

}  // anonymous namespace

// =============================================================================
// 公共接口
// =============================================================================

std::vector<SEConfig> ConfigParser::parseFile(const std::string& json_path) {
  std::ifstream file(json_path);
  if (!file.is_open()) {
    s_last_error = "Cannot open file: " + json_path;
    throw std::runtime_error(s_last_error);
  }

  json root;
  try {
    file >> root;
  } catch (const json::parse_error& e) {
    s_last_error = std::string("JSON parse error: ") + e.what();
    throw std::runtime_error(s_last_error);
  }

  return parseString(root.dump());
}

std::vector<SEConfig> ConfigParser::parseString(const std::string& json_content) {
  json root;
  try {
    root = json::parse(json_content);
  } catch (const json::parse_error& e) {
    s_last_error = std::string("JSON parse error: ") + e.what();
    throw std::runtime_error(s_last_error);
  }

  std::vector<SEConfig> configs;
  json entities = root.value("supervised_entities", json::array());
  for (const auto& item : entities) {
    configs.push_back(parseEntityConfig(item));
  }
  return configs;
}

bool ConfigParser::validate(const std::string& json_path) {
  try {
    auto configs = parseFile(json_path);
    for (const auto& cfg : configs) {
      if (cfg.name.empty()) {
        s_last_error = "Entity name is empty";
        return false;
      }
      for (const auto& mc : cfg.monitors) {
        if (mc.interval.count() <= 0) {
          s_last_error = "Invalid interval for monitor in entity: " + cfg.name;
          return false;
        }
      }
    }
    return true;
  } catch (const std::exception& e) {
    s_last_error = e.what();
    return false;
  }
}

std::string ConfigParser::lastError() { return s_last_error; }

std::string ConfigParser::monitorTypeToString(MonitorType type) {
  switch (type) {
    case MonitorType::PROCESS_LIFECYCLE: return "process_lifecycle";
    case MonitorType::RESOURCE: return "resource";
    case MonitorType::DEADLOCK: return "deadlock";
    case MonitorType::ALIVE: return "alive";
    case MonitorType::CUSTOM: return "custom";
  }
  return "custom";
}

}  // namespace phm
}  // namespace faw