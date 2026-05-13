#include <map>
#include <mutex>

#include "faw/phm/monitor.h"

namespace faw {
namespace phm {

// MonitorFactory 实现的简单注册表
using FactoryRegistry = std::map<MonitorType, MonitorFactoryFunc>;

static FactoryRegistry& registry() {
  static FactoryRegistry reg;
  return reg;
}

static std::mutex& registryMutex() {
  static std::mutex mtx;
  return mtx;
}

void MonitorFactory::registerFactory(MonitorType type,
                                     MonitorFactoryFunc factory) {
  std::lock_guard<std::mutex> lock(registryMutex());
  registry()[type] = factory;
}

std::unique_ptr<IMonitor> MonitorFactory::create(MonitorType type,
                                                 const std::string& name,
                                                 const MonitorConfig& cfg) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto it = registry().find(type);
  if (it != registry().end()) {
    return it->second(name, cfg);
  }
  return nullptr;
}

std::vector<MonitorType> MonitorFactory::registeredTypes() {
  std::lock_guard<std::mutex> lock(registryMutex());
  std::vector<MonitorType> types;
  for (const auto& [type, _] : registry()) {
    types.push_back(type);
  }
  return types;
}

}  // namespace phm
}  // namespace faw