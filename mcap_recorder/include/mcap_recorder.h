#pragma once
#include <signal.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cyber_to_mcap_converter.h"

namespace mcap {
class McapWriter;  // 前向声明
}

// namespace gwm {
// namespace adcos {
// namespace cyber {
// class Node;
// class ReaderBase;
// class ChannelManager;
// class Timer;
// namespace message {
// class RawMessage;
// }  // namespace message
// }  // namespace cyber
// }  // namespace adcos
// } // namespace gwm

// namespace gwm {
namespace apollo {
namespace cyber {
class Node;
class ReaderBase;
class ChannelManager;
class Timer;
namespace message {
class RawMessage;
}  // namespace message
}  // namespace cyber
}  // namespace adcos
// } // namespace gwm
using namespace apollo;
// using namespace gwm::adcos;
using MessageBase = cyber::message::RawMessage;
// ---------- MessageItem ----------
struct MessageItem {
  std::string topic;
  std::shared_ptr<MessageBase> msg;  // 原始消息指针，避免拷贝
};

// ---------- ChannelInfo ----------
struct ChannelInfo {
  std::string topic;
  std::string message_type;
  std::string proto_desc;  // mcap proto desc
  // Additional metadata if needed
};

// ---------- RecordingConfig ----------
struct RecordingConfig {
  std::string output_file;
  std::set<std::string> white_channels;   // 白名单
  std::set<std::string> black_channels;   // 黑名单
  bool record_all = false;                // 是否录制所有channel
  int discovery_interval_ms = 2000;       // 发现间隔
  uint64_t segment_interval_seconds = 0;  // 分段间隔（0表示不分段）
  uint64_t start_time_ns = 0;             // 开始时间
};

// ---------- McapRecorder ----------
class McapRecorder {
public:
  McapRecorder(const RecordingConfig& config);
  ~McapRecorder();

  bool start();
  void stop();
  void run();  // 主运行循环

private:
  // 内部方法
  bool initialize();
  void discoveryLoop();
  void writerLoop();
  void cleanup();

  // Channel管理
  void addChannel(
    const std::string& topic, const std::string& message_type, const std::string& proto_desc);
  void removeChannel(const std::string& topic);
  bool shouldRecordChannel(const std::string& topic) const;

  // 消息处理
  void onMessage(const std::string& topic, const std::shared_ptr<MessageBase>& msg);
  void writeMessageToMcap(const MessageItem& message);

  // 分段录制
  void rotateSegmentIfNeeded();
  void startNewSegment();

private:
  RecordingConfig config_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopped_{false};

  // Cyber相关
  std::shared_ptr<cyber::Node> node_;
  std::shared_ptr<cyber::ChannelManager> channel_manager_;
  std::unordered_map<std::string, std::shared_ptr<cyber::ReaderBase>> readers_;

  // 线程管理
  std::thread writer_thread_;
  std::shared_ptr<cyber::Timer> discovery_timer_;

  // 数据队列
  std::queue<MessageItem> message_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  // Channel管理
  std::unordered_map<std::string, ChannelInfo> channels_;
  std::mutex channels_mutex_;
  std::set<std::string> logged_filtered_channels_;  // 已记录的被过滤的 channel（避免重复打印）

  // MCAP相关
  std::shared_ptr<mcap::McapWriter> writer_;
  std::string current_segment_file_;
  uint64_t current_segment_start_time_ = 0;
  uint32_t segment_counter_ = 0;  // 分段计数器
  std::string base_timestamp_;    // 基础时间戳（用于分段录制时保持一致）

  // Schema和Channel缓存（每个segment都需要重新创建）
  std::unordered_map<std::string, uint16_t> schema_cache_;   // SchemaId
  std::unordered_map<std::string, uint16_t> channel_cache_;  // ChannelId

  // 统计信息
  std::atomic<uint64_t> total_messages_{0};
  std::atomic<uint64_t> total_bytes_{0};
  std::atomic<uint64_t> latest_record_time_ns_{0};
};
