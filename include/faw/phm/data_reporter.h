/**
 * @file data_reporter.h
 * @brief 数据上报器：本地落盘 + 云端上传
 * */

#ifndef FAW_PHM_DATA_REPORTER_H
#define FAW_PHM_DATA_REPORTER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "faw/phm/types.h"

namespace faw {
namespace phm {

/**
 * 一条监控采样数据记录
 * */
struct DataRecord {
  std::string se_name;                      ///< SE 名称
  std::string monitor_name;                 ///< Monitor 名称
  std::string monitor_type;                 ///< Monitor 类型字符串
  SeState state{SeState::INIT};             ///< 当前状态
  int64_t timestamp_ms{0};                  ///< 采样时刻 (unix ms)

  /**
   * 数值型指标
   * */
  std::map<std::string, double> metrics;

  /**
   * 扩展原始数据（JSON 字符串，存放 metrics 无法表达的复杂信息）
   * */
  std::string raw_data;
};

/**
 * 云端上传器抽象接口
 *
 * 负责将经过本地落盘的数据上传到云端。
 * 当前阶段仅提供接口定义，上传逻辑待后续实现。
 * */
class Uploader {
 public:
  virtual ~Uploader() = default;

  /**
   * 初始化上传器
   * @param config 上传配置（JSON 字符串，具体格式由子类定义）
   * @return true 初始化成功
   * */
  virtual bool initialize(const std::string& config) = 0;

  /**
   * 上传一条记录
   * @param record 数据记录
   * @return true 上传成功
   * */
  virtual bool upload(const DataRecord& record) = 0;

  /**
   * 批量上传
   * @param records 数据记录列表
   * @return true 全部上传成功
   * */
  virtual bool uploadBatch(const std::vector<DataRecord>& records) = 0;

  /**
   * 获取上传器状态
   * @return true 上传器可用
   * */
  virtual bool isAvailable() const noexcept = 0;

  /**
   * 获取上传器名称
   * */
  virtual const std::string& name() const noexcept = 0;
};

/**
 * 数据上报器
 *
 * 负责将 Monitor 采样数据写入本地文件（JSON Lines 格式），
 * 并通过 Uploader 接口送往云端。
 *
 * 文件格式：JSON Lines (.jsonl)
 * 文件命名：<data_dir>/<se_name>/YYYY-MM-DD-HH.jsonl
 * 文件轮转：每小时一个文件，自动创建目录
 * */
class DataReporter {
 public:
  /**
   * @param data_dir 数据根目录，默认为 /var/log/phmd/data
   * */
  explicit DataReporter(const std::string& data_dir = "/var/log/phmd/data");
  ~DataReporter();

  DataReporter(const DataReporter&) = delete;
  DataReporter& operator=(const DataReporter&) = delete;

  /**
   * 上报一条记录（同步写入本地文件）
   * @param record 数据记录
   * */
  void report(const DataRecord& record);

  /**
   * 批量上报
   * @param records 数据记录列表
   * */
  void reportBatch(const std::vector<DataRecord>& records);

  /**
   * 设置云端上传器
   * @param uploader 上传器唯一指针
   * */
  void setUploader(std::unique_ptr<Uploader> uploader);

  /**
   * 获取当前数据目录
   * */
  const std::string& dataDir() const noexcept;

  /**
   * 获取当日数据量统计（字节数）
   * */
  uint64_t todayBytes() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_DATA_REPORTER_H