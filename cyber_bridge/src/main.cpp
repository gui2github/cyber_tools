#include "FoxgloveServer.hpp"
#include "MessageConverter.hpp"
#ifdef USE_CYBER_BRIDGE
#include <cyber/cyber.h>

#include "CyberBridge.hpp"
#else
#include "FastDDSBridge.hpp"
#endif
#include <logger/log.h>

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
// #include <gperftools/heap-profiler.h>
// #include <gperftools/malloc_extension.h>
// #include <gperftools/profiler.h>

using namespace apollo;
// using namespace gwm::adcos;
#include <iostream>
#include <sstream>
#include <string>

class ArgParser {
public:
  ArgParser(int argc, const char* argv[])
      : programName(argv[0])
      , ipAddress("127.0.0.1")
      , port(8765) {
    parse(argc, argv);
  }

  const std::string& getIpAddress() const {
    return ipAddress;
  }
  int getPort() const {
    return port;
  }

  bool requestedHelp() const {
    return helpRequested;
  }
  bool hasError() const {
    return parseError;
  }
  const std::string& getErrorMessage() const {
    return errorMessage;
  }
  bool shouldShowUsageWithDefaults() const {
    return (!ipProvided && !portProvided);
  }

  void printHelp() const {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -i, --ipAddress <ip>   Foxglove server address (default 127.0.0.1)\n";
    std::cout << "  -p, --port <port>      Foxglove server port (default 8765)\n";
    std::cout << "  -h, --help             Show this help message\n";
  }

private:
  std::string programName;
  std::string ipAddress;
  int port;
  bool helpRequested{false};
  bool ipProvided{false};
  bool portProvided{false};
  bool parseError{false};
  std::string errorMessage;

  static bool isLongOpt(const std::string& arg, const std::string& name) {
    return arg == "--" + name || arg.rfind("--" + name + "=", 0) == 0;
  }

  static bool consumeValue(int argc, const char* argv[], int& index, std::string& out) {
    if (index + 1 < argc) {
      out = argv[++index];
      return true;
    }
    return false;
  }

  void parse(int argc, const char* argv[]) {
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-h" || arg == "--help") {
        helpRequested = true;
        continue;
      }

      if (arg == "-i" || isLongOpt(arg, "ipAddress")) {
        std::string value;
        if (arg.rfind("--ipAddress=", 0) == 0) {
          value = arg.substr(std::string("--ipAddress=").size());
        } else if (!consumeValue(argc, argv, i, value)) {
          parseError = true;
          errorMessage = "Missing value for " + arg;
          continue;
        }
        if (!value.empty()) {
          ipAddress = value;
          ipProvided = true;
        }
        continue;
      }

      if (arg == "-p" || isLongOpt(arg, "port")) {
        std::string value;
        if (arg.rfind("--port=", 0) == 0) {
          value = arg.substr(std::string("--port=").size());
        } else if (!consumeValue(argc, argv, i, value)) {
          parseError = true;
          errorMessage = "Missing value for " + arg;
          continue;
        }
        try {
          port = std::stoi(value);
          portProvided = true;
        } catch (...) {
          parseError = true;
          errorMessage = "Invalid port value '" + value + "'";
        }
        continue;
      }

      // 未知参数
      parseError = true;
      errorMessage = "Unknown option: " + arg;
    }
  }
};

int main(int argc, const char* argv[]) {
//   INIT_LOG_INSTANCE;
    // 可选：只显示 WARN 及以上级别
    SetLogLevel(LogLevel::WARN);

  ArgParser parser(argc, argv);
  if (parser.hasError()) {
    std::cerr << parser.getErrorMessage() << "\n";
    parser.printHelp();
    return 1;
  }
  if (parser.requestedHelp()) {
    parser.printHelp();
    return 0;
  }
  if (parser.shouldShowUsageWithDefaults()) {
    parser.printHelp();
    // 继续使用默认值运行
  }
  LOG_INFO << "Starting Foxglove Server";
#ifdef USE_CYBER_BRIDGE
  cyber::Init("fox_bridge");
#endif
  auto server = std::make_shared<FoxgloveServer>();

  bool nresult = server->getBridge()->start();
  if (!nresult) {
    LOG_ERROR << "Failed to start bridge";
    return -1;
  }
  LOG_INFO << "Bridge started";
  nresult = server->start(parser.getIpAddress(), parser.getPort());
  if (!nresult) {
    LOG_ERROR << "Failed to start server";
    return -1;
  }
#ifdef USE_CYBER_BRIDGE
  cyber::WaitForShutdown();
  cyber::Clear();
#endif
  return 0;
}
