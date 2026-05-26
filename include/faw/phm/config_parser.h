#ifndef FAW_PHM_CONFIG_PARSER_H
#define FAW_PHM_CONFIG_PARSER_H

#include <string>
#include <vector>

#include "faw/phm/types.h"

namespace faw {
namespace phm {

/**
 * 配置解析结果
 * */
struct ParseResult {
  PhmConfig global;                ///< 全局配置
  std::vector<SEConfig> entities;  ///< SE 配置列表
};

/**
 * 配置解析器
 *
 * 解析 JSON 格式的 SE 配置文件。
 * 使用 nlohmann/json 单头文件库进行解析。
 * */
class ConfigParser {
 public:
  /**
   * @brief 从 JSON 文件解析
   * @param json_path JSON 文件路径
   * @return ParseResult（包含 global 配置和 SE 配置列表）
   * @throws std::runtime_error 解析失败时抛出
   * */
  static ParseResult parseFile(const std::string& json_path);

  /**
   * @brief 从 JSON 字符串解析
   * @param json_content JSON 内容字符串
   * @return ParseResult（包含 global 配置和 SE 配置列表）
   * @throws std::runtime_error 解析失败时抛出
   * */
  static ParseResult parseString(const std::string& json_content);

  /**
   * @brief 校验 JSON 配置文件合法性
   * @param json_path JSON 文件路径
   * @return true 校验通过
   * */
  static bool validate(const std::string& json_path);

  /**
   * @brief 获取最后解析错误信息
   * */
  static std::string lastError();

  /**
   * @brief 将 MonitorType 转为字符串
   * */
  static std::string monitorTypeToString(MonitorType type);

 private:
  ConfigParser() = delete;

  static thread_local std::string s_last_error;
};

}  // namespace phm
}  // namespace faw

#endif  // FAW_PHM_CONFIG_PARSER_H