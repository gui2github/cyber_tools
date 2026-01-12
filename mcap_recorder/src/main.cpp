#include <logger/log.h>
#include <signal.h>

#include <iostream>
#include <string>

#include "arg_parser.h"
#include "cyber_to_mcap_converter.h"
#include "mcap_player.h"
#include "mcap_recorder.h"
#include "mcap_to_cyber_converter.h"
// 辅助函数：获取文件扩展名
std::string getFileExtension(const std::string& filename) {
  size_t dotPos = filename.find_last_of('.');
  if (dotPos != std::string::npos) {
    return filename.substr(dotPos + 1);
  }
  return "";
}

int main(int argc, const char* argv[]) {
//   INIT_LOG_INSTANCE;

  if (argc < 2) {
    std::cerr << "Error: No command specified\n\n";
    ArgParser parser(0, nullptr);
    parser.printHelp(argv[0]);
    return 1;
  }

  std::string command = argv[1];

  // 重新解析参数，跳过命令
  const char* new_argv[argc];
  new_argv[0] = argv[0];
  for (int i = 2; i < argc; i++) {
    new_argv[i - 1] = argv[i];
  }

  ArgParser parser(argc - 1, new_argv);

  // 自动转换模式
  if (command == "convert" || command == "--input") {
    // 如果第一个参数是 --input，说明没有指定命令，直接使用自动转换
    if (command == "--input") {
      // 重新解析所有参数
      ArgParser autoParser(argc, argv);
      autoParser.addShortOption("h", "help");
      autoParser.reparse();

      autoParser.addOptional("help", "Show help message");
      autoParser.addRequired("input", "Input file");
      autoParser.addRequired("output", "Output file");

      if (autoParser.has("help") || !autoParser.checkRequired()) {
        autoParser.printHelp(argv[0]);
        return 1;
      }

      std::string inputFile = autoParser.get("input");
      std::string outputFile = autoParser.get("output");

      std::string inputExt = getFileExtension(inputFile);
      std::string outputExt = getFileExtension(outputFile);

      // 根据文件扩展名自动选择转换方向
      if (inputExt == "record" && outputExt == "mcap") {
        LOG_INFO << "Auto-detected: Cyber record to MCAP conversion";
        CyberToMcapConverter converter;
        return converter.convert(inputFile, outputFile) ? 0 : 1;
      } else if (inputExt == "mcap" && outputExt == "record") {
        LOG_INFO << "Auto-detected: MCAP to Cyber record conversion";
        McapToCyberConverter converter;
        return converter.convert(inputFile, outputFile) ? 0 : 1;
      } else {
        LOG_ERROR << "Cannot auto-detect conversion direction. Please check file extensions.";
        LOG_ERROR << "Input file: " << inputFile << " (extension: " << inputExt << ")";
        LOG_ERROR << "Output file: " << outputFile << " (extension: " << outputExt << ")";
        LOG_ERROR << "Supported conversions: .record -> .mcap or .mcap -> .record";
        return 1;
      }
    } else {
      // 使用 convert 命令
      parser.addShortOption("h", "help");
      parser.reparse();

      parser.addOptional("help", "Show help message");
      parser.addRequired("input", "Input file");
      parser.addRequired("output", "Output file");

      if (parser.has("help") || !parser.checkRequired()) {
        parser.printHelp(argv[0]);
        return 1;
      }

      std::string inputFile = parser.get("input");
      std::string outputFile = parser.get("output");
      LOG_INFO << "Input file: " << inputFile;
      std::string inputExt = getFileExtension(inputFile);
      std::string outputExt = getFileExtension(outputFile);
      LOG_INFO << "Input file: " << inputFile << " (extension: " << inputExt << ")";
      // 根据文件扩展名自动选择转换方向
      if (inputExt == "record" && outputExt == "mcap") {
        LOG_INFO << "Auto-detected: Cyber record to MCAP conversion";
        CyberToMcapConverter converter;
        return converter.convert(inputFile, outputFile) ? 0 : 1;
      } else if (inputExt == "mcap" && outputExt == "record") {
        LOG_INFO << "Auto-detected: MCAP to Cyber record conversion";
        McapToCyberConverter converter;
        return converter.convert(inputFile, outputFile) ? 0 : 1;
      } else {
        LOG_ERROR << "Cannot auto-detect conversion direction. Please check file extensions.";
        LOG_ERROR << "Input file: " << inputFile << " (extension: " << inputExt << ")";
        LOG_ERROR << "Output file: " << outputFile << " (extension: " << outputExt << ")";
        LOG_ERROR << "Supported conversions: .record -> .mcap or .mcap -> .record";
        return 1;
      }
    }

  } else if (command == "record") {
    // 添加短选项映射
    parser.addShortOption("h", "help");
    parser.addShortOption("o", "output");
    parser.addShortOption("c", "white-channel");
    parser.addShortOption("k", "black-channel");
    parser.addShortOption("i", "segment-interval");
    parser.reparse();

    parser.addOptional("help", "Show help message");
    parser.addOptional("output", "Output mcap file (default: timestamp-based filename)");
    parser.addOptional("white-channel", "Only record specified channels (space-separated)");
    parser.addOptional("black-channel", "Do not record specified channels (space-separated)");
    parser.addOptional("segment-interval", "Record segmented every n second(s)");
    parser.addOptional("discovery-interval", "Channel discovery interval in ms (default: 2000)");

    if (parser.has("help")) {
      parser.printHelp(argv[0]);
      return 1;
    }

    // 构建录制配置
    RecordingConfig config;
    config.output_file = parser.get("output", "");  // 默认为空，使用时间戳命名
    config.record_all = true;                       // 默认录制所有
    config.discovery_interval_ms = parser.getInt("discovery-interval", 2000);
    config.segment_interval_seconds = parser.getInt("segment-interval", 0);

    // 处理白名单（支持一次使用后跟多个 topic，空格分隔）
    if (parser.has("white-channel")) {
      auto white_channels = parser.getAll("white-channel");
      for (const auto& channel : white_channels) {
        config.white_channels.insert(channel);
      }
      // 如果设置了白名单，则不再是"录制所有"
      if (!config.white_channels.empty()) {
        config.record_all = false;
      }
    }

    // 处理黑名单（支持一次使用后跟多个 topic，空格分隔）
    if (parser.has("black-channel")) {
      auto black_channels = parser.getAll("black-channel");
      for (const auto& channel : black_channels) {
        config.black_channels.insert(channel);
      }
    }

    McapRecorder recorder(config);
    if (recorder.start()) {
      recorder.run();
      return 0;
    } else {
      return 1;
    }

  } else if (command == "play") {
    // 添加短选项映射
    parser.addShortOption("h", "help");
    parser.addShortOption("l", "loop");
    parser.addShortOption("c", "white-channel");
    parser.addShortOption("k", "black-channel");
    parser.addShortOption("r", "rate");
    parser.addShortOption("s", "start");

    // 重新解析以应用短选项映射
    parser.reparse();

    parser.addOptional("help", "Show help message");
    parser.addOptional("white-channel", "Only play the specified channels (space-separated)");
    parser.addOptional("black-channel", "Do not play the specified channels (space-separated)");
    parser.addOptional("loop", "Loop play");
    parser.addOptional("rate", "Multiply the play rate by FACTOR (default: 1.0)");
    parser.addOptional("start", "Start playback from specified second (default: 0)");

    if (parser.has("help")) {
      parser.printHelp(argv[0]);
      return 1;
    }

    // 从位置参数中提取 .mcap 文件
    std::vector<std::string> mcap_files;
    for (const auto& arg : parser.getPositionalArgs()) {
      std::string ext = getFileExtension(arg);
      if (ext == "mcap") {
        mcap_files.push_back(arg);
      }
    }

    if (mcap_files.empty()) {
      LOG_ERROR << "No mcap files specified";
      parser.printHelp(argv[0]);
      return 1;
    }

    // 构建播放配置
    PlaybackConfig config;
    config.play_all = true;  // 默认播放所有
    config.speed_factor = std::stod(parser.get("rate", "1.0"));
    config.loop = parser.has("loop");
    config.start_offset = std::stod(parser.get("start", "0.0"));

    // 处理白名单（支持多次使用 -c 选项，用空格分隔）
    if (parser.has("white-channel")) {
      auto white_channels = parser.getAll("white-channel");
      for (const auto& channel : white_channels) {
        config.white_channels.insert(channel);
      }
      // 如果设置了白名单，则不再是"播放所有"
      if (!config.white_channels.empty()) {
        config.play_all = false;
      }
    }

    // 处理黑名单（支持多次使用 -k 选项，用空格分隔）
    if (parser.has("black-channel")) {
      auto black_channels = parser.getAll("black-channel");
      for (const auto& channel : black_channels) {
        config.black_channels.insert(channel);
      }
    }

    McapPlayer player;

    // 按顺序播放所有 mcap 文件
    for (size_t i = 0; i < mcap_files.size(); ++i) {
      config.input_file = mcap_files[i];
      std::cout << "Playing file " << (i + 1) << "/" << mcap_files.size() << ": " << mcap_files[i]
                << std::endl;

      if (player.play(config)) {
        player.run();
        player.stop();
      } else {
        LOG_ERROR << "Failed to play file: " << mcap_files[i];
        return 1;
      }
    }

    return 0;

  } else if (command == "help" || command == "--help" || command == "-h") {
    ArgParser parser(0, nullptr);
    parser.printHelp(argv[0]);
    return 0;

  } else {
    // 如果没有指定命令，尝试自动转换
    ArgParser autoParser(argc, argv);
    autoParser.addShortOption("h", "help");
    autoParser.reparse();

    autoParser.addOptional("help", "Show help message");
    autoParser.addRequired("input", "Input file");
    autoParser.addRequired("output", "Output file");

    if (autoParser.has("help") || !autoParser.checkRequired()) {
      autoParser.printHelp(argv[0]);
      return 1;
    }

    std::string inputFile = autoParser.get("input");
    std::string outputFile = autoParser.get("output");

    std::string inputExt = getFileExtension(inputFile);
    std::string outputExt = getFileExtension(outputFile);

    // 根据文件扩展名自动选择转换方向
    if (inputExt == "record" && outputExt == "mcap") {
      LOG_INFO << "Auto-detected: Cyber record to MCAP conversion";
      CyberToMcapConverter converter;
      return converter.convert(inputFile, outputFile) ? 0 : 1;
    } else if (inputExt == "mcap" && outputExt == "record") {
      LOG_INFO << "Auto-detected: MCAP to Cyber record conversion";
      McapToCyberConverter converter;
      return converter.convert(inputFile, outputFile) ? 0 : 1;
    } else {
      LOG_ERROR << "Cannot auto-detect conversion direction. Please check file extensions.";
      LOG_ERROR << "Input file: " << inputFile << " (extension: " << inputExt << ")";
      LOG_ERROR << "Output file: " << outputFile << " (extension: " << outputExt << ")";
      LOG_ERROR << "Supported conversions: .record -> .mcap or .mcap -> .record";
      return 1;
    }
  }

  return 0;
}
