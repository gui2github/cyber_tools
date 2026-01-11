#pragma once
#define USE_CYBER_BRIDGE
#include <atomic>
#include <foxglove/channel.hpp>
#include <foxglove/mcap.hpp>
#include <foxglove/server.hpp>
#include <foxglove/server/service.hpp>
#include <functional>
#include <memory>
#include <set>
#include <string>

#ifdef USE_CYBER_BRIDGE
class CyberBridge;
struct Schema;
#else
class FastDDSBridge;
#endif
class FoxgloveServer {
public:
  struct ChannelConfig {
    std::string type;
    int32_t sub_count;
    std::shared_ptr<foxglove::RawChannel> channel;
    ChannelConfig()
        : type("")
        , sub_count(0)
        , channel(nullptr) {}
  };

public:
  FoxgloveServer();
  ~FoxgloveServer();
  bool start(const std::string& ipAddress, uint16_t port);
  bool start(const std::string& filePath, const std::string& filename);
  void stop();

  bool createChannel(const std::string& topic, Schema& schema);
  void closeChannel(const std::string& topic);
  bool sendMessage(const std::string& topic, const std::string& message);
  auto getBridge() const {
    return _bridge;
  }
  bool createService(
    const std::string& service_name, Schema& request_schema, Schema& response_schema);

private:
  bool setConfigParam();  // 设置配置参数

private:
#ifdef USE_CYBER_BRIDGE
  std::shared_ptr<CyberBridge> _bridge;
#else
  std::shared_ptr<FastDDSBridge> _bridge;
#endif
  std::unique_ptr<foxglove::WebSocketServer> _server;
  std::unique_ptr<foxglove::McapWriter> _mcapWriter;
  std::map<std::string, ChannelConfig> _channels;
  std::map<uint32_t, std::string> _client_channels;
  // std::map<std::string, foxglove::Parameter> param_store;
  std::map<std::string, std::shared_ptr<foxglove::Parameter>> _param_store;  // 参数存储
  std::set<std::string> _services_set;

  std::atomic<bool> _isRecording;
  std::string _recordingFilePath;
  uint32_t _recordTime;  // 记录时间
};
