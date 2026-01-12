#include "mcap_player.h"

#include <cyber/cyber.h>
#include <cyber/message/protobuf_factory.h>
#include <fcntl.h>
#include <logger/log.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mcap/mcap.hpp>
#include <memory>
#include <sstream>
#include <thread>

#include "common.hpp"

using namespace std::chrono;
// ---- McapPlayer implementation ----

// 全局指针，用于信号处理
static McapPlayer* g_player_instance = nullptr;

static inline void MySigintHandler(int signum) {
  LOG_DEBUG << strsignal(signum) << " is received";

  // 确保 player 正确停止
  if (g_player_instance) {
    g_player_instance->stop();
  }

  cyber::Clear();
  cyber::WaitForShutdown();
  signal(signum, SIG_DFL);  // 恢复默认信号处理
}

McapPlayer::McapPlayer() {
  LOG_DEBUG << "McapPlayer initialized";
  cyber::Init("mcap_player");
  node_ = cyber::CreateNode("mcap_player");
  g_player_instance = this;
  signal(SIGINT, MySigintHandler);
  signal(SIGTERM, MySigintHandler);  // SIGKILL 无法捕获，改为 SIGTERM
  signal(SIGQUIT, MySigintHandler);
}

McapPlayer::~McapPlayer() {
  if (g_player_instance == this) {
    g_player_instance = nullptr;
  }
  stop();
}

bool McapPlayer::play(const std::string& inputFile, double speed) {
  PlaybackConfig config;
  config.input_file = inputFile;
  config.speed_factor = speed;
  config.play_all = true;

  return play(config);
}

bool McapPlayer::play(const PlaybackConfig& config) {
  config_ = config;

  if (running_.exchange(true)) {
    LOG_WARN << "McapPlayer is already running";
    return true;
  }

  // 重置状态标志（用于播放多个文件）
  stopped_ = false;
  paused_ = false;
  playback_done_ = false;
  total_messages_ = 0;
  total_bytes_ = 0;

  if (!initialize()) {
    LOG_ERROR << "Failed to initialize McapPlayer";
    running_ = false;
    return false;
  }

  config_.start_time_ns =
    duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();

  // 启动读取线程
  reader_thread_ = std::thread(&McapPlayer::readerLoop, this);

  // 启动键盘监听线程
  keyboard_thread_ = std::thread(&McapPlayer::keyboardListenerLoop, this);

  LOG_DEBUG << "McapPlayer started successfully";
  return true;
}

void McapPlayer::stop() {
  // 设置 running_ 为 false（即使已经是 false）
  bool was_running = running_.exchange(false);

  // 通知所有线程停止
  stopped_ = true;

  // 等待读取线程结束（即使 was_running 是 false，也要 join 线程）
  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }

  // 等待键盘监听线程结束
  if (keyboard_thread_.joinable()) {
    keyboard_thread_.join();
  }

  // 清理资源
  if (was_running) {
    cleanup();
    LOG_DEBUG << "McapPlayer stopped. Total messages: " << total_messages_
         << ", Total bytes: " << total_bytes_;
  }
}

void McapPlayer::run() {
  if (!running_) {
    LOG_ERROR << "McapPlayer is not running. Call play() first.";
    return;
  }
  auto last_status_time = steady_clock::now();

  // 等待停止信号或播放完成
  while (running_ && !playback_done_) {
    std::this_thread::sleep_for(milliseconds(50));

    // stop() 可能在信号处理里把 running_ 置为 false，或 readerLoop 设置 playback_done_
    if (!running_ || playback_done_) {
      break;
    }

    auto now = steady_clock::now();
    if (now - last_status_time >= milliseconds(50)) {
      uint64_t record_time_ns = current_playback_log_time_ns_.load();
      double record_time_sec = record_time_ns > 0 ? static_cast<double>(record_time_ns) / 1e9 : 0.0;
      double progress_sec = 0.0;
      if (earliest_log_time_ns_ > 0 && record_time_ns >= earliest_log_time_ns_) {
        progress_sec = static_cast<double>(record_time_ns - earliest_log_time_ns_) / 1e9;
      }
      double total_sec =
        total_duration_ns_ > 0 ? static_cast<double>(total_duration_ns_) / 1e9 : 0.0;

      std::ostringstream status;
      status << "[PLAYING] Record Time: " << std::fixed << std::setprecision(3) << record_time_sec
             << "    Progress: " << std::fixed << std::setprecision(3) << progress_sec << " / "
             << std::fixed << std::setprecision(3) << total_sec;
      if (paused_) {
        status << " [PAUSED]";
      }
      status << "    ";

      std::cout << "\r" << status.str() << std::flush;
      last_status_time = now;
    }
  }

  std::cout << std::endl;
}

bool McapPlayer::initialize() {
  // 初始化MCAP reader
  reader_ = std::make_shared<mcap::McapReader>();
  auto status = reader_->open(config_.input_file);
  if (!status.ok()) {
    LOG_ERROR << "Failed to open MCAP file: " << status.message;
    return false;
  }

  // 读取文件信息
  status = reader_->readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
  if (!status.ok()) {
    LOG_ERROR << "Failed to read MCAP summary: " << status.message;
    return false;
  }

  auto stats = reader_->statistics();
  if (stats) {
    earliest_log_time_ns_ = stats->messageStartTime;
    latest_log_time_ns_ = stats->messageEndTime;
    total_duration_ns_ =
      latest_log_time_ns_ > earliest_log_time_ns_ ? latest_log_time_ns_ - earliest_log_time_ns_ : 0;
    expected_total_messages_ = stats->messageCount;
    std::cout << "earliest_begin_time: " << earliest_log_time_ns_
              << ", latest_end_time: " << latest_log_time_ns_
              << ", total_msg_num: " << expected_total_messages_ << std::endl;
    std::cout << std::endl;
  } else {
    std::cout << "MCAP summary statistics not available." << std::endl;
    std::cout << std::endl;
    earliest_log_time_ns_ = 0;
    latest_log_time_ns_ = 0;
    total_duration_ns_ = 0;
    expected_total_messages_ = 0;
  }

  std::cout << "Please wait 3 second(s) for loading..." << std::endl;
  std::cout << "Hit Ctrl+C to stop, Space to pause, or 's' to step." << std::endl;
  std::cout << std::endl;

  // 获取所有channel信息，注册 desc 到cyber ，创建 writer
  auto channels = reader_->channels();
  auto schemas = reader_->schemas();
  static const auto cyber_factory = cyber::message::ProtobufFactory::Instance();
  for (const auto& [channel_id, channel] : channels) {
    // 检查是否应该播放这个channel
    if (!shouldPlayChannel(channel->topic)) {
      LOG_DEBUG << "Skipping channel: " << channel->topic;
      continue;
    }
    // 获取schema信息,注册 desc 到cyber
    auto schema = schemas[channel->schemaId];
    if (!schema) {
      LOG_WARN << "No schema found for channel: " << channel->topic;
      continue;
    }
    if (schema->encoding != "protobuf") {
      LOG_WARN << "Unsupported encoding: " << schema->encoding;
      continue;
    }
    // 检查是否已被注册
    if (!cyber_factory->FindMessageTypeByName(schema->name)) {
      std::string protoDesc(
        reinterpret_cast<const char*>(schema->data.data()), schema->data.size());
      std::string proto_desc_str = FdSetStringToCyberProtoDescString(protoDesc);
      if (proto_desc_str.empty()) {
        LOG_WARN << "Failed to convert proto desc to fd set string";
        continue;
      }
      if (!cyber_factory->RegisterMessage(proto_desc_str)) {
        LOG_WARN << "Failed to register message: " << schema->name;
        continue;
      }
      LOG_DEBUG << "Registered message: " << schema->name;
    }
    // 创建writer
    if (node_) {
      cyber::proto::RoleAttributes attr;
      attr.set_channel_name(channel->topic);
      attr.set_message_type(schema->name);
      attr.mutable_qos_profile()->set_depth(3);
      attr.mutable_qos_profile()->set_history(cyber::proto::QosHistoryPolicy::HISTORY_KEEP_ALL);
      attr.mutable_qos_profile()->set_reliability(
        cyber::proto::QosReliabilityPolicy ::RELIABILITY_BEST_EFFORT);
      auto writer = node_->CreateWriter<MessageBase>(attr);
      writers_[channel->topic] = writer;
      LOG_DEBUG << "Added channel for playback: " << channel->topic;
    }
  }
  // LOG_INFO << "McapPlayer initialized successfully";
  return true;
}

void McapPlayer::readerLoop() {
  LOG_DEBUG << "Reader thread started";

  // 获取消息迭代器
  auto messageView = reader_->readMessages();

  uint64_t first_message_time = 0;
  uint64_t playback_start_time =
    duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
  uint64_t start_offset_ns = static_cast<uint64_t>(config_.start_offset * 1e9);  // 转换为纳秒
  bool offset_applied = false;

  for (const auto& message : messageView) {
    if (!running_ || stopped_) {
      break;
    }
    std::string topic = message.channel->topic;
    // 检查是否应该播放这个channel
    if (writers_.find(topic) == writers_.end()) {
      continue;
    }

    // 记录第一条消息的时间
    if (first_message_time == 0) {
      first_message_time = message.message.logTime;
    }

    // 计算消息相对时间
    uint64_t message_relative_time = message.message.logTime - first_message_time;

    // 如果设置了起始偏移，跳过偏移时间之前的消息
    if (start_offset_ns > 0 && message_relative_time < start_offset_ns) {
      continue;
    }

    // 第一次应用偏移后，调整播放起始时间
    if (!offset_applied && start_offset_ns > 0) {
      playback_start_time =
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
      offset_applied = true;
      LOG_INFO << "Starting playback from " << config_.start_offset << " seconds";
    }

    // 计算播放时间（减去偏移量）
    uint64_t adjusted_relative_time = message_relative_time - start_offset_ns;
    uint64_t target_playback_time =
      playback_start_time + static_cast<uint64_t>(adjusted_relative_time / config_.speed_factor);

    // 处理暂停状态
    bool stepped_once = false;
    while (paused_ && running_ && !stopped_) {
      if (step_once_.exchange(false)) {
        stepped_once = true;
        break;
      }
      std::this_thread::sleep_for(milliseconds(100));
      // 暂停期间需要调整播放起始时间，避免恢复后快进
      playback_start_time += duration_cast<nanoseconds>(milliseconds(100)).count();
    }

    // 等待到正确的播放时间
    uint64_t current_time =
      duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    if (target_playback_time > current_time) {
      uint64_t sleep_time_ns = target_playback_time - current_time;
      std::this_thread::sleep_for(nanoseconds(sleep_time_ns));
    }

    // 创建RawMessage
    auto raw_msg = std::make_shared<MessageBase>();
    raw_msg->message.assign(
      reinterpret_cast<const char*>(message.message.data), message.message.dataSize);
    raw_msg->timestamp = message.message.publishTime;

    current_playback_log_time_ns_ = message.message.logTime;

    // 发布消息
    publishMessage(topic, raw_msg);

    // 更新统计
    total_messages_++;
    total_bytes_ += message.message.dataSize;

    if (stepped_once) {
      paused_ = true;
    }
  }

  LOG_DEBUG << "Reader thread stopped";

  // 如果设置了循环播放，重新开始
  if (config_.loop && running_) {
    LOG_DEBUG << "Looping playback...";
    cleanup();
    initialize();
    readerLoop();
  } else {
    // 播放结束，设置完成标志
    playback_done_ = true;
    std::cout << std::endl;
    std::cout << "Playback finished." << std::endl;
  }
}

bool McapPlayer::shouldPlayChannel(const std::string& topic) const {
  // 1. 首先检查黑名单（黑名单优先级最高）
  if (config_.black_channels.find(topic) != config_.black_channels.end()) {
    return false;
  }

  // 2. 如果设置了白名单，只播放白名单中的topic
  if (!config_.white_channels.empty()) {
    return config_.white_channels.find(topic) != config_.white_channels.end();
  }

  // 3. 如果没有设置白名单，则根据 play_all 决定（默认播放所有）
  return config_.play_all;
}

void McapPlayer::publishMessage(const std::string& topic, const std::shared_ptr<MessageBase>& msg) {
  auto write = std::dynamic_pointer_cast<cyber::Writer<MessageBase>>(writers_[topic]);
  write->Write(msg);
}

void McapPlayer::pause() {
  if (!paused_.exchange(true)) {
    LOG_DEBUG << "Playback paused. Messages: " << total_messages_ << ", Bytes: " << total_bytes_;
  }
}

void McapPlayer::resume() {
  if (paused_.exchange(false)) {
    LOG_DEBUG << "Playback resumed. Messages: " << total_messages_ << ", Bytes: " << total_bytes_;
  }
}

void McapPlayer::keyboardListenerLoop() {
  LOG_DEBUG << "Keyboard listener thread started";

  // 设置终端为非阻塞模式
  struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);  // 关闭行缓冲和回显
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);

  // 设置非阻塞
  int oldflags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldflags | O_NONBLOCK);

  while (running_ && !stopped_) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) > 0) {
      if (ch == ' ') {  // 空格键
        if (paused_) {
          resume();
        } else {
          pause();
        }
      } else if (ch == 's' || ch == 'S') {
        if (!paused_) {
          pause();
        }
        step_once_ = true;
        LOG_DEBUG << "Step requested.";
      }
    }
    std::this_thread::sleep_for(milliseconds(50));
  }

  // 恢复终端设置
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldflags);

  LOG_DEBUG << "Keyboard listener thread stopped";
}

void McapPlayer::cleanup() {
  // 关闭reader
  if (reader_) {
    reader_->close();
    reader_.reset();
  }

  // 清理writers
  writers_.clear();
  current_playback_log_time_ns_ = 0;
  earliest_log_time_ns_ = 0;
  latest_log_time_ns_ = 0;
  total_duration_ns_ = 0;
  expected_total_messages_ = 0;
  step_once_ = false;
  LOG_DEBUG << "McapPlayer cleanup completed";
}
