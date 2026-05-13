#ifndef FAW_PHM_LOGGER_H
#define FAW_PHM_LOGGER_H

/// @file logger.h
/// @brief PHM 轻量日志接口

#include <cstdarg>
#include <string>

namespace faw {
namespace phm {

/// PHM 内部日志器（单例，线程安全）
class Logger {
 public:
  /// 日志级别
  enum Level : uint8_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

  /// 获取全局实例
  static Logger& instance();

  /// 设置最低日志级别（低于此级别的不输出）
  void setLevel(Level lv);

  /// 获取当前日志级别
  Level getLevel() const noexcept;

  /// 设置日志输出路径（空 = stderr）
  /// @param path 日志文件路径，设为空字符串则输出到 stderr
  /// @return true 设置成功，false 打开文件失败
  bool setOutput(const std::string& path);

  /// 记录日志（printf 风格）
  void log(Level lv, const char* file, int line, const char* func,
           const char* fmt, ...) __attribute__((format(printf, 6, 7)));

  /// 记录日志（va_list 风格）
  void vlog(Level lv, const char* file, int line, const char* func,
            const char* fmt, va_list args);

 private:
  Logger();
  ~Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  class Impl;
  Impl* impl_;
};

// 便捷宏
#define PHM_LOG_DEBUG(fmt, ...)                                       \
  faw::phm::Logger::instance().log(faw::phm::Logger::DEBUG, __FILE__, \
                                   __LINE__, __func__, fmt, ##__VA_ARGS__)
#define PHM_LOG_INFO(fmt, ...)                                                 \
  faw::phm::Logger::instance().log(faw::phm::Logger::INFO, __FILE__, __LINE__, \
                                   __func__, fmt, ##__VA_ARGS__)
#define PHM_LOG_WARN(fmt, ...)                                                 \
  faw::phm::Logger::instance().log(faw::phm::Logger::WARN, __FILE__, __LINE__, \
                                   __func__, fmt, ##__VA_ARGS__)
#define PHM_LOG_ERROR(fmt, ...)                                       \
  faw::phm::Logger::instance().log(faw::phm::Logger::ERROR, __FILE__, \
                                   __LINE__, __func__, fmt, ##__VA_ARGS__)

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_LOGGER_H