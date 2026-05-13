#include "faw/phm/logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>

namespace faw {
namespace phm {

class Logger::Impl {
 public:
  Impl() : level_(Logger::INFO), file_(nullptr) {}

  ~Impl() {
    if (file_) {
      std::fclose(file_);
    }
  }

  void setLevel(Level lv) { level_ = lv; }
  Level getLevel() const noexcept { return level_; }

  bool setOutput(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (file_) {
      std::fclose(file_);
      file_ = nullptr;
    }

    if (path.empty()) {
      return true;  // 使用 stderr
    }

    file_ = std::fopen(path.c_str(), "a");
    return file_ != nullptr;
  }

  void log(Level lv, const char* file, int line, const char* func,
           const char* fmt, va_list args) {
    if (lv < level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 时间戳
    char timebuf[32];
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    // 级别标签
    const char* level_str = "DBG";
    switch (lv) {
      case DEBUG:
        level_str = "DBG";
        break;
      case INFO:
        level_str = "INF";
        break;
      case WARN:
        level_str = "WRN";
        break;
      case ERROR:
        level_str = "ERR";
        break;
    }

    // 格式化和输出
    FILE* out = file_ ? file_ : stderr;

    std::fprintf(out, "[%s] [%s] %s:%d:%s: ", timebuf, level_str, file, line,
                 func);
    std::vfprintf(out, fmt, args);
    std::fprintf(out, "\n");
    std::fflush(out);
  }

 private:
  Level level_;
  FILE* file_;
  std::mutex mutex_;
};

// =============================================================================
// Logger 单例
// =============================================================================

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

Logger::Logger() : impl_(new Impl()) {}

Logger::~Logger() { delete impl_; }

void Logger::setLevel(Level lv) { impl_->setLevel(lv); }
Logger::Level Logger::getLevel() const noexcept { return impl_->getLevel(); }

bool Logger::setOutput(const std::string& path) {
  return impl_->setOutput(path);
}

void Logger::log(Level lv, const char* file, int line, const char* func,
                 const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  impl_->log(lv, file, line, func, fmt, args);
  va_end(args);
}

void Logger::vlog(Level lv, const char* file, int line, const char* func,
                  const char* fmt, va_list args) {
  impl_->log(lv, file, line, func, fmt, args);
}

}  // namespace phm
}  // namespace faw