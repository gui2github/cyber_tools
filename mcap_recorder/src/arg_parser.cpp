#include "arg_parser.h"

ArgParser::ArgParser(int argc, const char* argv[])
    : argc_(argc) {
  // 保存原始参数
  for (int i = 0; i < argc; ++i) {
    argv_store_.push_back(argv[i]);
  }
  parse(argc, argv);
}

void ArgParser::addRequired(const std::string& key, const std::string& description) {
  required.insert(key);
  helpInfo[key] = description;
}

void ArgParser::addOptional(const std::string& key, const std::string& description) {
  helpInfo[key] = description;
}

void ArgParser::addShortOption(const std::string& short_opt, const std::string& long_opt) {
  short_to_long[short_opt] = long_opt;
}

void ArgParser::reparse() {
  // 清空之前解析的结果
  args.clear();
  multi_args.clear();
  positional_args.clear();

  // 重新解析
  std::vector<const char*> argv_ptrs;
  for (const auto& arg : argv_store_) {
    argv_ptrs.push_back(arg.c_str());
  }
  parse(argc_, argv_ptrs.data());
}

std::string ArgParser::get(const std::string& key, const std::string& default_val) const {
  auto it = args.find(key);
  return it != args.end() ? it->second : default_val;
}

int ArgParser::getInt(const std::string& key, int default_val) const {
  auto it = args.find(key);
  try {
    return it != args.end() ? std::stoi(it->second) : default_val;
  } catch (...) {
    return default_val;
  }
}

bool ArgParser::getBool(const std::string& key, bool default_val) const {
  auto it = args.find(key);
  if (it != args.end()) {
    if (it->second.empty()) return true;
    return it->second == "1" || it->second == "true";
  }
  return default_val;
}

bool ArgParser::has(const std::string& key) const {
  return args.find(key) != args.end() || multi_args.find(key) != multi_args.end();
}

std::vector<std::string> ArgParser::getAll(const std::string& key) const {
  auto it = multi_args.find(key);
  if (it != multi_args.end()) {
    return it->second;
  }

  // 如果没有多值，尝试返回单值
  auto single_it = args.find(key);
  if (single_it != args.end() && !single_it->second.empty()) {
    return {single_it->second};
  }

  return {};
}

bool ArgParser::checkRequired() const {
  for (const auto& key : required) {
    if (!has(key)) return false;
  }
  return true;
}

void ArgParser::printHelp(const std::string& programName) const {
  std::cout << "Usage: " << programName << " <command> [OPTIONS] [FILES]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  record             Record cyber data to mcap format\n";
  std::cout << "  convert            Convert between cyber record and mcap format (auto-detect)\n";
  std::cout << "  play               Play mcap file(s) through cyber\n\n";

  if (!helpInfo.empty()) {
    std::cout << "Options:\n";
    for (const auto& pair : helpInfo) {
      // 查找对应的短选项
      std::string short_opt;
      for (const auto& s2l : short_to_long) {
        if (s2l.second == pair.first) {
          short_opt = s2l.first;
          break;
        }
      }

      std::cout << "  ";
      if (!short_opt.empty()) {
        std::cout << "-" << short_opt << ", ";
      }
      std::cout << "--" << pair.first;
      if (required.count(pair.first)) std::cout << " (required)";
      if (!pair.second.empty()) std::cout << " : " << pair.second;
      std::cout << "\n";
    }
    std::cout << "\n";
  }

  std::cout << "Examples:\n";
  std::cout << "  Record:\n";
  std::cout << "    " << programName << " record\n";
  std::cout << "    " << programName << " record -o data.mcap\n";
  std::cout << "    " << programName << " record -c /topic1 /topic2\n";
  std::cout << "    " << programName << " record -o data -i 3600 -c /topic1 -k /debug\n\n";
  std::cout << "  Play:\n";
  std::cout << "    " << programName << " play file.mcap\n";
  std::cout << "    " << programName << " play file1.mcap file2.mcap -l -r 2.0\n";
  std::cout << "    " << programName << " play data.mcap -c /topic1 /topic2 -k /debug\n";
  std::cout << "    " << programName << " play data.mcap -s 10 -r 2.0\n";
  std::cout << "    Press SPACE during playback to pause/resume\n\n";
  std::cout << "  Convert:\n";
  std::cout << "    " << programName << " convert --input record.record --output record.mcap\n";
  std::cout << "    " << programName << " convert --input data.mcap --output data.record\n";
}

void ArgParser::parse(int argc, const char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg.rfind("--", 0) == 0) {
      // 长选项: --key
      arg = arg.substr(2);
      size_t eq_pos = arg.find('=');
      if (eq_pos != std::string::npos) {
        // 格式: --key=value
        std::string key = arg.substr(0, eq_pos);
        std::string value = arg.substr(eq_pos + 1);
        args[key] = value;
      } else {
        // 格式: --key value1 value2 value3 ...
        // 收集所有后续的非选项参数
        std::vector<std::string> values;
        while (i + 1 < argc && argv[i + 1][0] != '-') {
          values.push_back(argv[i + 1]);
          i++;
        }

        if (values.empty()) {
          args[arg] = "";  // 仅有 --flag，视为 true
        } else if (values.size() == 1) {
          args[arg] = values[0];  // 单个值存储到 args
        } else {
          multi_args[arg] = values;  // 多个值存储到 multi_args
        }
      }
    } else if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
      // 短选项: -k
      std::string short_opt = arg.substr(1);

      // 查找对应的长选项
      auto it = short_to_long.find(short_opt);
      if (it != short_to_long.end()) {
        std::string long_opt = it->second;

        // 格式: -k value1 value2 value3 ...
        // 收集所有后续的非选项参数
        std::vector<std::string> values;
        while (i + 1 < argc && argv[i + 1][0] != '-') {
          values.push_back(argv[i + 1]);
          i++;
        }

        if (values.empty()) {
          args[long_opt] = "";  // 仅有 -flag，视为 true
        } else if (values.size() == 1) {
          args[long_opt] = values[0];  // 单个值存储到 args
        } else {
          multi_args[long_opt] = values;  // 多个值存储到 multi_args
        }
      } else {
        // 未注册的短选项，直接存储
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          args[short_opt] = argv[i + 1];
          i++;
        } else {
          args[short_opt] = "";
        }
      }
    } else {
      // 位置参数（非选项参数）
      positional_args.push_back(arg);
    }
  }
}
