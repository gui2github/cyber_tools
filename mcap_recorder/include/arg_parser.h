#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ArgParser {
public:
  ArgParser(int argc, const char* argv[]);

  void addRequired(const std::string& key, const std::string& description = "");
  void addOptional(const std::string& key, const std::string& description = "");
  void addShortOption(const std::string& short_opt, const std::string& long_opt);

  // 重新解析参数（在添加短选项映射后调用）
  void reparse();

  std::string get(const std::string& key, const std::string& default_val = "") const;
  int getInt(const std::string& key, int default_val = 0) const;
  bool getBool(const std::string& key, bool default_val = false) const;
  bool has(const std::string& key) const;
  bool checkRequired() const;

  // 获取同一选项的所有值（支持多次使用同一选项）
  std::vector<std::string> getAll(const std::string& key) const;

  void printHelp(const std::string& programName) const;

  const std::vector<std::string>& getPositionalArgs() const {
    return positional_args;
  }

private:
  std::unordered_map<std::string, std::string> args;
  std::unordered_map<std::string, std::vector<std::string>> multi_args;  // 支持多值选项
  std::unordered_map<std::string, std::string> helpInfo;
  std::unordered_map<std::string, std::string> short_to_long;  // 短选项到长选项的映射
  std::unordered_set<std::string> required;
  std::vector<std::string> positional_args;  // 位置参数（非选项参数）

  // 保存原始参数用于重新解析
  int argc_;
  std::vector<std::string> argv_store_;

  void parse(int argc, const char* argv[]);
};
