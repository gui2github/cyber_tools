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

namespace mcap {
class McapReader;  // 前向声明
}

namespace gwm {
namespace adcos {
namespace cyber {
class Node;
class WriterBase;
namespace message {
class RawMessage;
}  // namespace message
}  // namespace cyber
}  // namespace adcos
}  // namespace gwm

namespace apollo {
namespace cyber {
class Node;
class WriterBase;
namespace message {
class RawMessage;
}  // namespace message
}  // namespace cyber
} // namespace apollo

// using namespace gwm::adcos;
using namespace apollo;
using MessageBase = cyber::message::RawMessage;
// ---------- PlaybackConfig ----------
struct PlaybackConfig {
  std::string input_file;
  std::set<std::string> white_channels;  // 白名单
  std::set<std::string> black_channels;  // 黑名单
  bool play_all = false;                 // 是否播放所有channel
  double speed_factor = 1.0;             // 播放速度倍数
  bool loop = false;                     // 是否循环播放
  double start_offset = 0.0;             // 播放起始偏移（秒）
  uint64_t start_time_ns = 0;            // 开始时间
};

// ---------- McapPlayer ----------
class McapPlayer {
public:
  McapPlayer();
  ~McapPlayer();

  bool play(const std::string& inputFile, double speed = 1.0);
  bool play(const PlaybackConfig& config);
  void stop();
  void run();  // 主运行循环

  void pause();   // 暂停播放
  void resume();  // 恢复播放
  bool isPaused() const {
    return paused_;
  }

private:
  // 内部方法
  bool initialize();
  void readerLoop();
  void cleanup();
  void keyboardListenerLoop();  // 键盘监听线程

  // Channel管理
  bool shouldPlayChannel(const std::string& topic) const;

  // 消息处理
  void publishMessage(const std::string& topic, const std::shared_ptr<MessageBase>& msg);

private:
  PlaybackConfig config_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopped_{false};
  std::atomic<bool> paused_{false};         // 暂停状态
  std::atomic<bool> playback_done_{false};  // 播放完成标志

  // Cyber相关
  std::shared_ptr<cyber::Node> node_;
  std::unordered_map<std::string, std::shared_ptr<cyber::WriterBase>> writers_;

  // 线程管理
  std::thread reader_thread_;
  std::thread keyboard_thread_;  // 键盘监听线程

  // MCAP相关
  std::shared_ptr<mcap::McapReader> reader_;

  // Channel管理
  std::unordered_map<std::string, std::string> channel_message_types_;
  std::mutex channels_mutex_;

  // 统计信息
  std::atomic<uint64_t> total_messages_{0};
  std::atomic<uint64_t> total_bytes_{0};
  std::atomic<uint64_t> current_playback_log_time_ns_{0};
  uint64_t earliest_log_time_ns_ = 0;
  uint64_t latest_log_time_ns_ = 0;
  uint64_t total_duration_ns_ = 0;
  uint64_t expected_total_messages_ = 0;
  std::atomic<bool> step_once_{false};
};
