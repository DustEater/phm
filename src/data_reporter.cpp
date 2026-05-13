#include "faw/phm/data_reporter.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

#include "faw/phm/logger.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace faw {
namespace phm {

// =============================================================================
// 辅助：DataRecord 序列化为 JSON Lines 字符串
// =============================================================================

static std::string recordToJsonLine(const DataRecord& record) {
  json j;
  j["se"] = record.se_name;
  j["monitor"] = record.monitor_name;
  j["type"] = record.monitor_type;
  j["state"] = static_cast<int>(record.state);
  j["ts"] = record.timestamp_ms;

  // 数值指标
  if (!record.metrics.empty()) {
    json m = json::object();
    for (const auto& [key, val] : record.metrics) {
      m[key] = val;
    }
    j["metrics"] = m;
  }

  // 扩展数据
  if (!record.raw_data.empty()) {
    j["raw"] = record.raw_data;
  }

  return j.dump();
}

// =============================================================================
// 辅助：获取当前小时的 .jsonl 文件名
// =============================================================================

static std::string getHourlyFilename(const std::string& data_dir,
                                     const std::string& se_name) {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  struct tm local;
  localtime_r(&t, &local);

  std::ostringstream oss;
  oss << data_dir << "/" << se_name << "/" << std::setfill('0')
      << (1900 + local.tm_year) << "-" << std::setw(2) << (local.tm_mon + 1)
      << "-" << std::setw(2) << local.tm_mday << "-" << std::setw(2)
      << local.tm_hour << ".jsonl";
  return oss.str();
}

// =============================================================================
// 辅助：确保目录存在
// =============================================================================

static bool ensureDir(const std::string& path) {
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) return true;
    errno = ENOTDIR;
    return false;
  }
  if (mkdir(path.c_str(), 0755) == 0) return true;
  // 父目录不存在则递归创建
  if (errno == ENOENT) {
    size_t pos = path.rfind('/');
    if (pos != std::string::npos && pos > 0) {
      if (!ensureDir(path.substr(0, pos))) return false;
      return mkdir(path.c_str(), 0755) == 0;
    }
  }
  return false;
}

// =============================================================================
// DataReporter::Impl
// =============================================================================

class DataReporter::Impl {
 public:
  explicit Impl(const std::string& data_dir) : data_dir_(data_dir) {
    ensureDir(data_dir_);
  }

  ~Impl() {
    if (file_stream_.is_open()) {
      file_stream_.close();
    }
  }

  void report(const DataRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string filename = getHourlyFilename(data_dir_, record.se_name);
    openFile(filename);

    if (!file_stream_.is_open()) {
      PHM_LOG_WARN("DataReporter: cannot write to %s", filename.c_str());
      return;
    }

    std::string line = recordToJsonLine(record);
    file_stream_ << line << "\n";
    file_stream_.flush();

    bytes_today_ += line.size() + 1;

    // 如果设置了上传器，同步上传
    if (uploader_ && uploader_->isAvailable()) {
      uploader_->upload(record);
    }
  }

  void reportBatch(const std::vector<DataRecord>& records) {
    for (const auto& r : records) {
      report(r);
    }
  }

  void setUploader(std::unique_ptr<Uploader> uploader) {
    std::lock_guard<std::mutex> lock(mutex_);
    uploader_ = std::move(uploader);
  }

  const std::string& dataDir() const noexcept { return data_dir_; }

  uint64_t todayBytes() const noexcept { return bytes_today_; }

 private:
  void openFile(const std::string& filename) {
    // 如果当前文件已经是对的文件，无需切换
    if (file_stream_.is_open() && current_filename_ == filename) {
      return;
    }

    // 关闭旧文件
    if (file_stream_.is_open()) {
      file_stream_.close();
    }

    // 确保目录存在
    size_t sep = filename.rfind('/');
    if (sep != std::string::npos) {
      ensureDir(filename.substr(0, sep));
    }

    // 以追加模式打开
    file_stream_.open(filename, std::ios::app);
    if (file_stream_.is_open()) {
      current_filename_ = filename;
    }
  }

  std::string data_dir_;
  std::string current_filename_;
  std::ofstream file_stream_;
  std::unique_ptr<Uploader> uploader_;
  std::mutex mutex_;
  uint64_t bytes_today_{0};
};

// =============================================================================
// DataReporter 公共接口
// =============================================================================

DataReporter::DataReporter(const std::string& data_dir)
    : impl_(std::make_unique<Impl>(data_dir)) {}

DataReporter::~DataReporter() = default;

void DataReporter::report(const DataRecord& record) { impl_->report(record); }

void DataReporter::reportBatch(const std::vector<DataRecord>& records) {
  impl_->reportBatch(records);
}

void DataReporter::setUploader(std::unique_ptr<Uploader> uploader) {
  impl_->setUploader(std::move(uploader));
}

const std::string& DataReporter::dataDir() const noexcept {
  return impl_->dataDir();
}

uint64_t DataReporter::todayBytes() const noexcept {
  return impl_->todayBytes();
}

}  // namespace phm
}  // namespace faw