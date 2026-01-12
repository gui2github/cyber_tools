#include "mcap_recorder.h"

#include <cyber/cyber.h>
#include <cyber/message/protobuf_factory.h>
#include <logger/log.h>

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

// ---- McapRecorder implementation ----

// 全局指针，用于信号处理
static McapRecorder* g_recorder_instance = nullptr;

static inline void MySigintHandler(int signum) {
  LOG_DEBUG << strsignal(signum) << " is received";

  // 确保 recorder 正确关闭 writer
  if (g_recorder_instance) {
    g_recorder_instance->stop();
  }

  cyber::Clear();
  cyber::WaitForShutdown();
  signal(signum, SIG_DFL);  // 恢复默认信号处理
  // raise(signum);            // 重新触发信号
}

McapRecorder::McapRecorder(const RecordingConfig& config)
    : config_(config) {
  std::cout << "McapRecorder initialized with output: " << config_.output_file << std::endl;
  std::cout << "Discovery interval: " << config_.discovery_interval_ms << "ms" << std::endl;
  std::cout << "Segment interval: " << config_.segment_interval_seconds << "s" << std::endl;
  std::cout << "Record all: " << (config_.record_all ? "true" : "false") << std::endl;
  std::cout << "White channels: " << config_.white_channels.size() << std::endl;
  std::cout << "Black channels: " << config_.black_channels.size() << std::endl;
  std::cout << std::endl;
  cyber::Init("mcap_recorder");
  node_ = cyber::CreateNode("mcap_recorder");

  // 设置全局实例指针（用于信号处理）
  g_recorder_instance = this;

  signal(SIGINT, MySigintHandler);
  signal(SIGTERM, MySigintHandler);  // SIGTERM 可以捕获，SIGKILL 无法捕获
  signal(SIGQUIT, MySigintHandler);
}

McapRecorder::~McapRecorder() {
  // 清除全局实例指针
  if (g_recorder_instance == this) {
    g_recorder_instance = nullptr;
  }
  stop();
  // cyber::Clear();
  // cyber::WaitForShutdown();
}

bool McapRecorder::initialize() {
  // 初始化MCAP writer
  startNewSegment();

  LOG_INFO << "McapRecorder initialized successfully";
  return true;
}

bool McapRecorder::start() {
  if (running_.exchange(true)) {
    LOG_WARN << "McapRecorder is already running";
    return true;
  }

  if (!initialize()) {
    LOG_ERROR << "Failed to initialize McapRecorder";
    running_ = false;
    return false;
  }

  config_.start_time_ns =
    duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
  latest_record_time_ns_ = config_.start_time_ns;

  discoveryLoop();

  // 启动写入线程
  writer_thread_ = std::thread(&McapRecorder::writerLoop, this);

  LOG_INFO << "McapRecorder started successfully";
  return true;
}

void McapRecorder::stop() {
  if (!running_.exchange(false)) {
    return;
  }
  // 停止发现定时器
  if (discovery_timer_) {
    discovery_timer_->Stop();
  }

  // 通知所有线程停止
  stopped_ = true;
  queue_cv_.notify_all();

  // 等待写入线程结束
  if (writer_thread_.joinable()) {
    writer_thread_.join();
  }

  // 清理资源
  cleanup();
  std::cout << std::endl;
  std::cout << "McapRecorder stopped. Total messages: " << total_messages_
            << ", Total bytes: " << total_bytes_ << std::endl;
}

void McapRecorder::run() {
  if (!running_) {
    LOG_ERROR << "McapRecorder is not running. Call start() first.";
    return;
  }

  std::cout << "McapRecorder is running. Press Ctrl+C to stop." << std::endl;
  std::cout << "Please wait 3 second(s) for loading..." << std::endl;
  std::cout << std::endl;

  auto last_status_time = steady_clock::now();

  // 等待停止信号
  while (running_) {
    std::this_thread::sleep_for(milliseconds(50));
    // 检查是否需要分段
    rotateSegmentIfNeeded();

    // stop() 可能在信号处理里把 running_ 置为 false，这里及时退出，避免多打印一行
    if (!running_) {
      break;
    }

    auto now = steady_clock::now();
    if (now - last_status_time >= milliseconds(50)) {
      // 使用系统当前时间作为 Record Time，单位秒
      uint64_t record_time_ns =
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
      double record_time_sec = static_cast<double>(record_time_ns) / 1e9;

      size_t channel_count = 0;
      {
        std::lock_guard<std::mutex> lock(channels_mutex_);
        channel_count = channels_.size();
      }

      std::ostringstream status;
      status << "[RUNNING] Record Time: " << std::fixed << std::setprecision(0) << record_time_sec
             << "    Progress: " << channel_count << " channels, " << total_messages_.load()
             << " messages    ";

      std::cout << "\r" << status.str() << std::flush;
      last_status_time = now;
    }
  }

  std::cout << std::endl;
}

void McapRecorder::discoveryLoop() {
  // LOG_INFO << "Discovery loop started";

  // 初始化Cyber拓扑管理器
  auto topology = cyber::service_discovery::TopologyManager::Instance();
  if (!topology) {
    LOG_ERROR << "Failed to get TopologyManager instance";
    return;
  }

  // 等待拓扑管理器初始化
  std::this_thread::sleep_for(seconds(2));
  discovery_timer_ = std::make_shared<cyber::Timer>(
    config_.discovery_interval_ms,
    [this, topology]() {
      try {
        std::lock_guard<std::mutex> lock(channels_mutex_);

        // 获取所有channel
        auto channel_manager = topology->channel_manager();
        std::vector<std::string> current_topics;
        channel_manager->GetChannelNames(&current_topics);

        // 检查需要移除的channel
        std::vector<std::string> channels_to_remove;
        for (const auto& [topic, info] : channels_) {
          if (std::find(current_topics.begin(), current_topics.end(), topic) ==
              current_topics.end()) {
            channels_to_remove.push_back(topic);
          }
        }

        // 移除不存在的channel
        for (const auto& topic : channels_to_remove) {
          removeChannel(topic);
          LOG_INFO << "Removed channel: " << topic;
        }

        // 检查新channel
        for (const auto& topic : current_topics) {
          // 检查是否是新channel
          if (channels_.find(topic) == channels_.end()) {
            // 检查是否应该录制这个channel
            if (shouldRecordChannel(topic)) {
              // 获取message type和proto desc
              std::string message_type;
              channel_manager->GetMsgType(topic, &message_type);
              std::string proto_desc;
              channel_manager->GetProtoDesc(topic, &proto_desc);
              std::string mcap_desc = CyberProtoDescStringToFdSetString(proto_desc);
              if (mcap_desc.empty()) {
                LOG_WARN << "Failed to convert proto desc to mcap desc for topic: " << topic;
                continue;
              }
              addChannel(topic, message_type, mcap_desc);
              LOG_INFO << "Discovered new channel: " << topic << " [" << message_type << "]";
            } else {
              // 只在第一次遇到被过滤的 channel 时打印日志
              if (logged_filtered_channels_.find(topic) == logged_filtered_channels_.end()) {
                LOG_DEBUG << "Skipping channel (filtered): " << topic;
                logged_filtered_channels_.insert(topic);
              }
            }
          }
        }

      } catch (const std::exception& e) {
        LOG_ERROR << "Error in discovery loop: " << e.what();
      }
    },
    false);  // oneshot = false，表示周期性执行

  // 启动定时器
  discovery_timer_->Start();
  LOG_INFO << "Discovery timer started with interval: " << config_.discovery_interval_ms << "ms";
}

void McapRecorder::writerLoop() {
  // LOG_INFO << "Writer thread started";

  while (running_ || !message_queue_.empty()) {
    MessageItem message;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait_for(lock, milliseconds(100), [this] {
        return !message_queue_.empty() || !running_;
      });

      if (message_queue_.empty()) {
        continue;
      }

      message = std::move(message_queue_.front());
      message_queue_.pop();
    }

    // 写入MCAP
    writeMessageToMcap(message);

    // 更新统计
    total_messages_++;
    if (message.msg) {
      total_bytes_ += message.msg->message.size();
    }
  }

  LOG_DEBUG << "Writer thread stopped";
}

void McapRecorder::addChannel(
  const std::string& topic, const std::string& message_type, const std::string& proto_desc) {
  // 创建channel信息
  ChannelInfo info;
  info.topic = topic;
  info.message_type = message_type;
  info.proto_desc = proto_desc;
  channels_[topic] = info;

  // 订阅该channel
  if (node_) {
    auto callback = [this, topic](const std::shared_ptr<MessageBase>& msg) {
      onMessage(topic, msg);
    };
    cyber::ReaderConfig config;
    config.channel_name = topic;
    config.qos_profile.set_depth(3);
    config.qos_profile.set_history(cyber::proto::QosHistoryPolicy::HISTORY_KEEP_ALL);
    config.qos_profile.set_reliability(cyber::proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
    config.qos_profile.set_durability(cyber::proto::QosDurabilityPolicy::DURABILITY_VOLATILE);
    auto reader = node_->CreateReader<MessageBase>(config, callback);
    readers_[topic] = reader;
    LOG_INFO << "Added channel: " << topic;
  }
}

void McapRecorder::removeChannel(const std::string& topic) {
  channels_.erase(topic);
  LOG_INFO << "Removed channel: " << topic;
}

bool McapRecorder::shouldRecordChannel(const std::string& topic) const {
  // 1. 首先检查黑名单（黑名单优先级最高）
  if (config_.black_channels.find(topic) != config_.black_channels.end()) {
    return false;
  }

  // 2. 如果设置了白名单，只录制白名单中的topic
  if (!config_.white_channels.empty()) {
    return config_.white_channels.find(topic) != config_.white_channels.end();
  }

  // 3. 如果没有设置白名单，则根据 record_all 决定（默认录制所有）
  return config_.record_all;
}

void McapRecorder::onMessage(const std::string& topic, const std::shared_ptr<MessageBase>& msg) {
  if (!running_) {
    return;
  }
  MessageItem message;
  message.topic = topic;
  message.msg = msg;

  if (msg) {
    latest_record_time_ns_ = msg->timestamp;
  }
  LOG_INFO << "Received message: " << topic << " [" << msg->message.size() << " bytes]";
  // 添加到队列
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.push(std::move(message));
    queue_cv_.notify_one();
  }
}

void McapRecorder::writeMessageToMcap(const MessageItem& message) {
  if (!writer_ || !message.msg) {
    return;
  }

  try {
    // 检查是否需要分段（基于时间）
    rotateSegmentIfNeeded();

    // 从ChannelInfo获取message_type和proto_desc
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto channel_it = channels_.find(message.topic);
    if (channel_it == channels_.end()) {
      LOG_WARN << "Channel not found for topic: " << message.topic;
      return;
    }

    const std::string& message_type = channel_it->second.message_type;
    const std::string& proto_desc = channel_it->second.proto_desc;

    // 获取或创建schema
    mcap::SchemaId schema_id;

    auto schema_it = schema_cache_.find(message_type);
    if (schema_it == schema_cache_.end()) {
      // 创建新的schema
      mcap::Schema schema(message_type, "protobuf", proto_desc);
      writer_->addSchema(schema);
      schema_id = schema.id;
      schema_cache_[message_type] = schema_id;
    } else {
      schema_id = schema_it->second;
    }

    // 获取或创建channel
    mcap::ChannelId channel_id;

    auto channel_cache_it = channel_cache_.find(message.topic);
    if (channel_cache_it == channel_cache_.end()) {
      // 创建新的channel
      mcap::Channel channel(message.topic, "protobuf", schema_id);
      channel.metadata["message_type"] = message_type;
      writer_->addChannel(channel);
      channel_id = channel.id;
      channel_cache_[message.topic] = channel_id;
    } else {
      channel_id = channel_cache_it->second;
    }

    // 写入消息
    mcap::Message mcap_msg;
    mcap_msg.channelId = channel_id;
    mcap_msg.sequence = 0;  // 如果需要可以维护序列号
    mcap_msg.publishTime = message.msg->timestamp;
    mcap_msg.logTime = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    mcap_msg.data = reinterpret_cast<const std::byte*>(message.msg->message.data());
    mcap_msg.dataSize = message.msg->message.size();

    auto write_status = writer_->write(mcap_msg);
    if (!write_status.ok()) {
      LOG_ERROR << "Failed to write message to " << message.topic << ": " << write_status.message;
    }

  } catch (const std::exception& e) {
    LOG_ERROR << "Error writing message to MCAP: " << e.what();
  }
}

void McapRecorder::rotateSegmentIfNeeded() {
  if (config_.segment_interval_seconds <= 0) {
    return;
  }

  uint64_t current_time = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

  if (current_time - current_segment_start_time_ >= config_.segment_interval_seconds) {
    startNewSegment();
  }
}

void McapRecorder::startNewSegment() {
  // 关闭当前writer
  if (writer_) {
    writer_->close();
    writer_.reset();
  }

  // 清空schema和channel缓存（新文件需要重新创建）
  schema_cache_.clear();
  channel_cache_.clear();

  // 生成基础时间戳（只在第一次生成）
  if (base_timestamp_.empty()) {
    auto now = system_clock::now();
    auto time_t = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);

    std::stringstream ts;
    ts << std::put_time(&tm, "%Y%m%d_%H%M%S");
    base_timestamp_ = ts.str();
  }

  // 生成新的文件名
  std::stringstream ss;

  // 如果没有指定输出文件名，使用时间戳
  if (config_.output_file.empty()) {
    ss << base_timestamp_;
    if (config_.segment_interval_seconds > 0) {
      ss << "_" << segment_counter_;
    }
  } else {
    // 使用指定的文件名
    ss << config_.output_file;
    if (config_.segment_interval_seconds > 0) {
      ss << "_" << segment_counter_;
    }
  }
  ss << ".mcap";

  current_segment_file_ = ss.str();
  current_segment_start_time_ =
    duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
  segment_counter_++;  // 增加分段计数器

  // 创建新的writer
  mcap::McapWriterOptions options("");
  options.compression = mcap::Compression::Zstd;  // 可以根据需要启用压缩
  // 使用库的默认 chunkSize，不限制 chunk 大小（只限制文件大小）

  writer_ = std::make_shared<mcap::McapWriter>();
  auto result = writer_->open(current_segment_file_, options);
  if (!result.ok()) {
    LOG_ERROR << "Failed to open MCAP file: " << result.message;
    writer_.reset();
    return;
  }

  std::cout << "Started new segment: " << current_segment_file_ << std::endl;
  std::cout << std::endl;
}

void McapRecorder::cleanup() {
  // 关闭writer（确保文件正确关闭，写入 footer 和 magic number）
  if (writer_) {
    try {
      writer_->close();
      LOG_INFO << "MCAP writer closed successfully";
    } catch (const std::exception& e) {
      LOG_ERROR << "Error closing MCAP writer: " << e.what();
    }
    writer_.reset();
  }

  // 清理channels
  channels_.clear();

  // 清空队列
  std::lock_guard<std::mutex> lock(queue_mutex_);
  while (!message_queue_.empty()) {
    message_queue_.pop();
  }

  LOG_INFO << "McapRecorder cleanup completed";
}
