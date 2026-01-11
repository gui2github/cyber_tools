#include "FoxgloveServer.hpp"

#include <foxglove/context.hpp>
#include <foxglove/error.hpp>
#include <foxglove/foxglove.hpp>
#include <foxglove/server/parameter.hpp>

#include "MessageConverter.hpp"
#ifdef USE_CYBER_BRIDGE
#include "CyberBridge.hpp"
#else
#include "FastDDSBridge.hpp"
#endif
#include <filesystem>
#include <nlohmann/json.hpp>

#include "logger/log.h"

using Json = nlohmann::json;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

std::vector<std::byte> makeBytes(std::string_view sv) {
  const auto* data = reinterpret_cast<const std::byte*>(sv.data());
  return {data, data + sv.size()};
}

FoxgloveServer::FoxgloveServer() {
  foxglove::setLogLevel(foxglove::LogLevel::Info);
#ifdef USE_CYBER_BRIDGE
  _bridge = std::make_unique<CyberBridge>();
#else
  _bridge = std::make_unique<FastDDSBridge>();
#endif
}

FoxgloveServer::~FoxgloveServer() {
  stop();
}

bool FoxgloveServer::start(const std::string& ipAddress, uint16_t port) {
  foxglove::WebSocketServerOptions ws_options;
  ws_options.name = "FoxgloveServer";
  ws_options.host = ipAddress;
  ws_options.port = port;
  // hack for foxglove studio
  std::map<std::string, std::string> serverInfo = {{"ROS_DISTRO", "humble"}};
  ws_options.server_info = std::move(serverInfo);
  ws_options.capabilities = foxglove::WebSocketServerCapabilities::ClientPublish |
                            foxglove::WebSocketServerCapabilities::ConnectionGraph |
                            foxglove::WebSocketServerCapabilities::Services |
                            foxglove::WebSocketServerCapabilities::Parameters;
  // ws_options.capabilities = foxglove::WebSocketServerCapabilities::Services;
  ws_options.supported_encodings = {"json", "protobuf"};
  ws_options.callbacks.onConnectionGraphSubscribe = []() {
    LOG_INFO << "Connection graph subscribed";
  };
  ws_options.callbacks.onConnectionGraphUnsubscribe = []() {
    LOG_INFO << "Connection graph unsubscribed";
  };
  ws_options.callbacks.onClientAdvertise = [this](uint32_t clientId,
                                             const foxglove::ClientChannel& channel) {
    _client_channels.insert(std::make_pair(channel.id, std::string(channel.topic)));
    _bridge->onWriterCreate(std::string(channel.topic), std::string(channel.schema_name));
    LOG_INFO << "Client id: " << clientId << " channel:" << channel.id << " topic:" << channel.topic
         << " type:" << channel.schema_name << " encoding:" << channel.encoding;
  };
  ws_options.callbacks.onClientUnadvertise = [this](uint32_t clientId, uint32_t clientChannelId) {
    if (_client_channels.find(clientChannelId) == _client_channels.end()) return;
    _bridge->onWriterDelete(_client_channels[clientChannelId]);
    _client_channels.erase(clientChannelId);
    LOG_INFO << "Client unadvertised: " << clientId << " " << clientChannelId;
  };
  ws_options.callbacks.onMessageData =
    [this](uint32_t clientId, uint32_t clientChannelId, const std::byte* data, size_t dataLen) {
      if (_client_channels.find(clientChannelId) == _client_channels.end()) {
        LOG_WARN << "Client channel not found: " << clientChannelId << " client id:" << clientId;
        return;
      }
      std::string msg = std::string(reinterpret_cast<const char*>(data), dataLen);
      _bridge->onReceiveMsg(_client_channels[clientChannelId], msg);
      LOG_INFO << "Received message: " << clientId << " " << clientChannelId << " " << dataLen
           << " bytes";
    };
  ws_options.callbacks.onSubscribe = [this](uint64_t channel_id,
                                       const foxglove::ClientMetadata& client) {
    for (auto& [topic, channel] : _channels) {
      LOG_INFO << "Subscribed to channel: " << channel_id << " client id:" << client.id
           << " name:" << topic;
      if (channel.channel->id() == channel_id) {
        if (channel.sub_count++ > 1) return;
        _bridge->onSubscribe(topic,
          std::bind(
            &FoxgloveServer::sendMessage, this, std::placeholders::_1, std::placeholders::_2));
        return;
      }
    }
  };
  ws_options.callbacks.onUnsubscribe = [this](uint64_t channel_id,
                                         const foxglove::ClientMetadata& client) {
    for (auto& [topic, channel] : _channels) {
      if (channel.channel->id() == channel_id) {
        LOG_INFO << "Unsubscribed from channel: " << channel_id << " client id:" << client.id
             << " name:" << topic;
        if (channel.sub_count-- > 0) return;
        _bridge->onUnsubscribe(topic);
        return;
      }
    }
  };
  ws_options.callbacks.onParametersSubscribe =
    [this](const std::vector<std::string_view>& parameterNames) {
      LOG_INFO << "Parameters subscribed: " << parameterNames.size();
      for (auto& name : parameterNames) {
        LOG_INFO << "Parameter Subscribe name: " << name;
      }
    };

  ws_options.callbacks.onParametersUnsubscribe =
    [this](const std::vector<std::string_view>& parameterNames) {
      LOG_INFO << "Parameters unsubscribed: " << parameterNames.size();
      for (auto& name : parameterNames) {
        LOG_INFO << "Parameter Unsubscribe name: " << name;
      }
    };
  ws_options.callbacks.onGetParameters =
    [this](uint32_t client_id [[maybe_unused]],
      std::optional<std::string_view>
        request_id,
      const std::vector<std::string_view>& param_names) -> std::vector<foxglove::Parameter> {
    std::vector<foxglove::Parameter> result;
    if (request_id.has_value()) {
      LOG_INFO << "onGetParameters called with request_id '" << *request_id;
    }
    std::vector<cyber::Parameter> params;
    _bridge->onGetParameter(param_names, params);
    for (auto& param : params) {
      switch (param.Type()) {
        case cyber::proto::ParamType::BOOL:
          result.emplace_back(param.Name(), param.AsBool());
          break;
        case cyber::proto::ParamType::INT:
          result.emplace_back(param.Name(), static_cast<int64_t>(param.AsInt64()));
          break;
        case cyber::proto::ParamType::DOUBLE:
          result.emplace_back(param.Name(), param.AsDouble());
          break;
        case cyber::proto::ParamType::STRING:
        case cyber::proto::ParamType::PROTOBUF:
          result.emplace_back(param.Name(), param.AsString());
          break;
        default:
          LOG_WARN << "Unsupported parameter type: " << param.TypeName();
          result.emplace_back(param.Name());
      }
    }
    return result;
  };
  ws_options.callbacks.onSetParameters =
    [this](uint32_t client_id [[maybe_unused]],
      std::optional<std::string_view>
        request_id,
      const std::vector<foxglove::ParameterView>& params) -> std::vector<foxglove::Parameter> {
    std::cerr << "onSetParameters called";
    if (request_id.has_value()) {
      std::cerr << " with request_id '" << *request_id << "'";
    }
    std::cerr << " for parameters:\n";
    std::vector<foxglove::Parameter> result;
    for (const auto& param : params) {
      std::cerr << " - " << param.name();
      const std::string name(param.name());
      if (auto it = _param_store.find(name); it != _param_store.end()) {
        if (name.find("read_only_") == 0) {
          std::cerr << " - not updated\n";
          result.emplace_back(it->second->clone());
        } else {
          std::cerr << " - updated\n";
          it->second = std::make_shared<foxglove::Parameter>(param.clone());
          result.emplace_back(param.clone());
        }
      }
    }
    return result;
  };

  auto server_result = foxglove::WebSocketServer::create(std::move(ws_options));
  if (!server_result.has_value()) {
    LOG_ERROR << "Failed to create server: " << foxglove::strerror(server_result.error());
    return false;
  }
  _server = std::make_unique<foxglove::WebSocketServer>(std::move(server_result.value()));
  LOG_INFO << "conncet to :" << ipAddress << ":" << port;
  _bridge->startDiscoverTimer(
    [this](const std::string& topic,
      Schema& schema_1,
      [[maybe_unused]] std::optional<Schema>& schema_2) {
      if (!createChannel(topic, schema_1)) {
        LOG_WARN << "Failed to create channel: " << topic;
      }
    },
    [this](const std::string& topic) {
      closeChannel(topic);
    },
    [this](const std::string& topic, Schema& schema_1, std::optional<Schema>& schema_2) {
      createService(topic, schema_1, schema_2.value());
    });
  return true;
}

bool FoxgloveServer::start(const std::string& filePath, const std::string& filename) {
  std::string full_filePath = std::filesystem::path(filePath).append(filename).string();
  foxglove::McapWriterOptions options;
  options.path = full_filePath;
  auto result = foxglove::McapWriter::create(options);
  if (!result.has_value()) {
    LOG_ERROR << "Failed to create MCAP writer: " << foxglove::strerror(result.error());
    return false;
  }
  _mcapWriter = std::make_unique<foxglove::McapWriter>(std::move(result.value()));
  LOG_INFO << "Created MCAP writer for file: " << full_filePath;
  return true;
}

void FoxgloveServer::stop() {
  _server->stop();
  _server.reset();
  _server = nullptr;
  _channels.clear();
  LOG_INFO << "Server stopped";
}

bool FoxgloveServer::createChannel(const std::string& topic, Schema& sch_data) {
  if (_channels.find(topic) != _channels.end()) {
    // LOG_ERROR << "Channel already exists: " << topic;
    return false;
  }
  foxglove::Schema schema;
  schema.name = sch_data.name;
  schema.data_len = sch_data.desc.length();
  schema.data = reinterpret_cast<const std::byte*>(sch_data.desc.c_str());
  schema.encoding = "protobuf";
  auto channel_result = foxglove::RawChannel::create(topic, "protobuf", schema);
  if (!channel_result.has_value()) {
    LOG_ERROR << "Failed to create channel: " << foxglove::strerror(channel_result.error());
    return false;
  }
  ChannelConfig channel_config;
  channel_config.type = sch_data.name;
  channel_config.channel =
    std::make_shared<foxglove::RawChannel>(std::move(channel_result.value()));
  _channels.insert({topic, std::move(channel_config)});
  LOG_INFO << "Created channel: " << topic << " with type: " << sch_data.name;

  if (MessageConverter::instance().hasConverter(sch_data.name) &&
      (_channels.find(topic + "/converted") == _channels.end())) {
    schema.name = MessageConverter::instance().getTargetTypeName(sch_data.name);
    std::string data = MessageConverter::instance().getTargetDescriptorString(sch_data.name);
    schema.data_len = data.length();
    schema.data = reinterpret_cast<const std::byte*>(data.c_str());
    channel_result = foxglove::RawChannel::create(topic + "/converted", "protobuf", schema);
    if (!channel_result.has_value()) {
      LOG_ERROR << "Failed to create channel: " << foxglove::strerror(channel_result.error());
      return false;
    }
    ChannelConfig converted_channel_config;
    converted_channel_config.type = sch_data.name;
    converted_channel_config.channel =
      std::make_shared<foxglove::RawChannel>(std::move(channel_result.value()));
    _channels.insert({topic + "/converted", std::move(converted_channel_config)});
    LOG_INFO << "Created converted channel: " << topic << " with type: " << schema.name;
  }
  return true;
}

void FoxgloveServer::closeChannel(const std::string& topic) {
  if (_channels.find(topic) == _channels.end()) {
    LOG_ERROR << "Channel not found: " << topic;
    return;
  }
  _channels.erase(topic);
  LOG_INFO << "Closed channel: " << topic;
}

bool FoxgloveServer::sendMessage(const std::string& topic, const std::string& message) {
  if (topic.empty() || message.empty()) {
    // LOG_ERROR << "topic or message is empty !" << topic;
    return false;
  }
  if (_channels.find(topic) == _channels.end()) {
    LOG_ERROR << "Channel not found: " << topic;
    return false;
  }
  auto& channel = _channels[topic];
  if (!channel.channel) {
    return false;
  }
  foxglove::FoxgloveError message_result =
    channel.channel->log(reinterpret_cast<const std::byte*>(message.c_str()), message.length());
  if (message_result != foxglove::FoxgloveError::Ok) {
    LOG_ERROR << "Failed to log message: " << foxglove::strerror(message_result);
    return false;
  }
  if (MessageConverter::instance().hasConverter(channel.type)) {
    auto& repeat = _channels[topic + "/converted"];
    std::string converted_message;
    MessageConverter::instance().convert(message, channel.type, converted_message);
    if (converted_message.empty() || !repeat.channel) {
      // LOG_WARN << "Failed convert message: " << channel.type;
      return false;
    }
    message_result = repeat.channel->log(
      reinterpret_cast<const std::byte*>(converted_message.c_str()), converted_message.length());
    if (message_result != foxglove::FoxgloveError::Ok) {
      LOG_ERROR << "Failed to log message: " << foxglove::strerror(message_result);
      return false;
    }
  }
  return true;
}

bool FoxgloveServer::createService(
  const std::string& topic, Schema& request_schema, Schema& response_schema) {
  if (_services_set.find(topic) != _services_set.end()) {
    LOG_ERROR << "Service already exists: " << topic;
    return false;
  }
  foxglove::ServiceSchema schema_s;
  schema_s.name = topic;
  foxglove::ServiceMessageSchema request, response;

  request.schema.name = "IntMathRequest";
  request.schema.data_len = request_schema.desc.length();
  request.schema.data = reinterpret_cast<const std::byte*>(request_schema.desc.c_str());
  request.schema.encoding = "jsonschema";
  request.encoding = "json";
  schema_s.request.emplace(request);

  response.schema.name = "IntMathResponse";
  response.schema.data_len = response_schema.desc.length();
  response.schema.data = reinterpret_cast<const std::byte*>(response_schema.desc.c_str());
  response.schema.encoding = "jsonschema";
  response.encoding = "json";
  schema_s.response.emplace(response);

  static foxglove::ServiceHandler handler = [this](const foxglove::ServiceRequest& request,
                                              foxglove::ServiceResponder&& responder) {
    std::string res_str;
    _bridge->onClientCall(request.service_name, std::string(request.payloadStr()), res_str);
    std::move(responder).respondOk(
      reinterpret_cast<const std::byte*>(res_str.c_str()), res_str.length());
  };
  _bridge->onClientRegister(topic);
  auto service_result = foxglove::Service::create(topic, schema_s, handler);
  if (!service_result.has_value()) {
    LOG_ERROR << "Failed to create service: " << foxglove::strerror(service_result.error());
    return false;
  }
  LOG_INFO << "Created service: " << topic << " with request schema: " << request_schema.desc
       << " and response schema: " << response.schema.name;
  auto error = _server->addService(std::move(*service_result));
  if (error != foxglove::FoxgloveError::Ok) {
    LOG_ERROR << "Failed to add service: " << foxglove::strerror(error);
    return false;
  }
  _services_set.insert(topic);
  return true;
}
