#include "CyberBridge.hpp"

#include <memory>
#include <vector>

#include "cyber/service_discovery/specific_manager/node_manager.h"
#include "protoPool.hpp"
#include "service_impl.hpp"

CyberBridge::CyberBridge() {}

CyberBridge::~CyberBridge() {
  stop();
}

void CyberBridge::startDiscoverTimer(const adCallback& topic_adCb,
  const unScribeCallback& topic_unadCb, const adCallback& service_adCb) {
  // init timer
  if (!_timer) {
    std::this_thread::sleep_for(std::chrono::seconds(2));  // wait for node manager init
    _timer = std::make_shared<cyber::Timer>(
      500,
      [&]() {
        discoverTopics(topic_adCb, topic_unadCb);
        discoverServices(service_adCb);
      },
      false);
    LOG_INFO << "create timer ";
  }
  _timer->Start();
}

void CyberBridge::discoverTopics(const adCallback& adCb, const unScribeCallback& unScribeCb) {
  auto topology = cyber::service_discovery::TopologyManager::Instance();
  // std::this_thread::sleep_for(std::chrono::seconds(2));
  auto channel_manager = topology->channel_manager();
  std::vector<std::string> topics;
  channel_manager->GetChannelNames(&topics);
  if (topics == _topics) {
    return;
  }
  _topics = topics;
  // 遍历_msg_manages并移除不在_topics中的元素
  std::unordered_set<std::string> topics_set(_topics.begin(), _topics.end());
  for (auto it = _msg_manages.begin(); it != _msg_manages.end();) {
    if (topics_set.find(it->first) == topics_set.end()) {
      onUnsubscribe(it->first);
      unScribeCb(it->first);
      LOG_INFO << "remove topic: " << it->first;
      it = _msg_manages.erase(it);
    } else {
      ++it;
    }
  }
  // 添加新的topics到_msg_manages
  for (const auto& channel : _topics) {
    if (_writers.find(channel) != _writers.end()) continue;
    if (_msg_manages.find(channel) == _msg_manages.end()) {
      auto message = std::make_shared<mssageManage>();
      if (!message->init_topic(channel)) {
        continue;
      }
      _msg_manages.insert({channel, message});
      // 只广播存在 writer 的 topic
      if (!channel_manager->HasWriter(channel)) continue;
      std::string shema_desc = _msg_manages[channel]->getFdSet();
      if (shema_desc.empty()) continue;
      Schema schema = {_msg_manages[channel]->getType(), shema_desc};
      std::optional<Schema> opt_schema = std::nullopt;
      adCb(channel, schema, opt_schema);
      LOG_INFO << "add topic: " << channel << " msg_type: " << _msg_manages[channel]->getType();
    }
  }
}

void CyberBridge::discoverServices(const adCallback& adCb) {
  auto topology = cyber::service_discovery::TopologyManager::Instance();
  auto service_manager = topology->service_manager();
  std::vector<std::string> srv_names;
  std::vector<cyber::RoleAttributes> services;
  service_manager->GetServers(&services);
  for (auto& service : services) {
    if (_services.find(service.service_name()) == _services.end()) {
      if (service_map_impl_.find(service.service_name()) != service_map_impl_.end()) {
        const auto& ser_type = service_map_impl_.at(service.service_name());
        auto request = std::make_shared<mssageManage>();
        bool nresult = request->init_type(ser_type.first);
        auto response = std::make_shared<mssageManage>();
        nresult |= response->init_type(ser_type.second);
        if (nresult) {
          Schema request_schema = {ser_type.first, request->getJsonSchema()};
          auto response_schema =
            std::optional<Schema>({ser_type.second, response->getJsonSchema()});
          adCb(service.service_name(), request_schema, response_schema);
          _services.emplace(service.service_name(), std::make_pair(request, response));
        } else {
          LOG_WARN << "service: " << service.service_name()
               << " create failed, please check the service_impl.hpp";
        }
      } else {
        LOG_WARN << "service: " << service.service_name()
             << " not found . please define it in service_impl.hpp";
      }
    }
  }
}

// 该 sub 为 cyber 侧的 pub 的 topic，bridge 端为 sub
void CyberBridge::onUnsubscribe(const std::string& topic) {
  // auto reader = _node->GetReader<MessageBase>(topic);
  if (_readers.find(topic) == _readers.end()) {
    return;
  }
  if (!_readers[topic]) {
    return;
  }
  auto reader = _readers[topic];
  reader->ClearData();
  reader->Shutdown();
  _node->DeleteReader(topic);
  _readers.erase(topic);
  LOG_INFO << "unsubscribe topic: " << topic;
}
// 该 sub 为 cyber 侧的 pub 的 topic，bridge 端为 sub
void CyberBridge::onSubscribe(const std::string& topic, const msgCallback& cb) {
  if (_readers.find(topic) != _readers.end() || _msg_manages[topic] == nullptr) {
    return;
  }
  auto callback = [this, topic, cb](const std::shared_ptr<MessageBase> msg) {
    cb(topic, _msg_manages[topic]->getMsgProtoString(msg));
  };
  auto reader = _node->CreateReader<MessageBase>(topic, callback);
  _readers[topic] = std::move(reader);
  LOG_INFO << "subscribe topic: " << topic;
}

void CyberBridge::onWriterCreate(const std::string& topic, const std::string& msg_type) {
  for (auto& it : _msg_manages) {
    if (!it.second) {
      continue;
    }
    if (it.second->getType() == msg_type) {
      _msg_manages.insert({topic, it.second});
      break;
    }
  }
  if (_msg_manages.find(topic) == _msg_manages.end()) {
    auto message = std::make_shared<mssageManage>();
    if (!message->init_type(msg_type)) {
      LOG_WARN << "subscribe topic: " << topic << " not found, please subscribe first";
      return;
    }
    _msg_manages.insert({topic, message});
  }
  if (_writers.find(topic) == _writers.end()) {
    cyber::proto::RoleAttributes attr;
    attr.set_channel_name(topic);
    attr.set_message_type(msg_type);
    auto writer = _node->CreateWriter<MessageBase>(attr);
    _writers[topic] = writer;
    LOG_INFO << "create writer for topic: " << topic << " msg_type: " << msg_type;
  } else {
    LOG_INFO << "writer for topic: " << topic << " already exists";
  }
}
void CyberBridge::onWriterDelete(const std::string& topic) {
  if (_msg_manages.find(topic) != _msg_manages.end()) {
    _msg_manages.erase(topic);
  }
  if (_writers.find(topic) != _writers.end()) {
    _writers.erase(topic);
    LOG_INFO << "delete writer for topic: " << topic;
  } else {
    LOG_INFO << "writer for topic: " << topic << " not found";
  }
}

void CyberBridge::onReceiveMsg(const std::string& topic, const std::string& msg) {
  if (_writers.find(topic) == _writers.end()) {
    LOG_WARN << "subscribe topic: " << topic << " not found, please subscribe first";
    return;
  }
  auto msg_manage = _msg_manages[topic];
  if (!msg_manage) {
    return;
  }
  auto raw_msg = std::make_shared<MessageBase>();
  if (!msg_manage->getMsgFromJsonString(msg, raw_msg)) {
    LOG_WARN << "receive msg for topic: " << topic << " msg_type: " << msg_manage->getType();
    return;
  }

  auto write = std::dynamic_pointer_cast<cyber::Writer<MessageBase>>(_writers[topic]);
  if (!write) {
    return;
  }
  write->Write(raw_msg);
  LOG_INFO << "receive msg for topic: " << topic << " msg_type: " << msg_manage->getType();
}

void CyberBridge::onServiceRegister(const std::string& service_name) {
  if (_services.find(service_name) != _services.end()) {
    return;
  }
  auto service = _node->CreateService<MessageBase, MessageBase>(service_name,
    std::bind(&CyberBridge::onServiceCall, this, std::placeholders::_1, std::placeholders::_2));
  // _services[service_name] = service;
  LOG_INFO << "register service: " << service_name;
}

void CyberBridge::onServiceCall([[maybe_unused]] const std::shared_ptr<MessageBase>& request,
  [[maybe_unused]] std::shared_ptr<MessageBase>& response) {}

void CyberBridge::onServiceUnregister(const std::string& service_name) {
  // _node->DeleteService(service_name);
  (void)service_name;
}

void CyberBridge::onClientRegister(const std::string& client_name) {
  if (_clients.find(client_name) != _clients.end()) {
    return;
  }
  auto client = _node->CreateClient<MessageBase, MessageBase>(client_name);
  _clients[client_name] = client;
  LOG_INFO << "register client: " << client_name;
}

void CyberBridge::onClientCall(const std::string& topic, const std::string& req, std::string& res) {
  if (_clients.find(topic) != _clients.end()) {
    auto& req_mgr = _services[topic].first;
    auto& resp_mgr = _services[topic].second;
    auto request = std::make_shared<MessageBase>();
    req_mgr->getMsgFromJsonString(req, request);
    auto response = std::make_shared<MessageBase>();
    auto client =
      std::dynamic_pointer_cast<cyber::Client<MessageBase, MessageBase>>(_clients[topic]);
    response = client->SendRequest(request);
    if (response) {
      LOG_INFO << "client call success :" << response->TypeName();
      res = resp_mgr->getMsgJsonString(response);
    } else {
      LOG_WARN << "client call failed";
      return;
    }

  } else {
    LOG_WARN << "client: " << topic << " not found";
  }
}

void CyberBridge::onClientUnregister(const std::string& client_name) {
  // _node->DeleteClient(client_name);
  (void)client_name;
}

// void CyberBridge::onCreateParamClient(const std::string& param_service_name)
// {
//     // create parameter
//     // if (!_param_server){
//     //     _param_server = std::make_shared<ParameterServer>(_node);
//     // }
//     if (!_param_client){
//         _param_client = std::make_shared<ParameterClient>(_node,param_service_name);
//     }
// }
// //  @param type string 、 int 、double 、bool 、protobuf
// void CyberBridge::onSetParameter(const std::string& key, const std::string& type, size_t size,
// const void* value)
// {
//     if (type == "string")
//     {
//         _param_client->SetParameter(parameter);
//     }

// }

void CyberBridge::onGetParameter(
  const std::vector<std::string_view>& param_names, std::vector<cyber::Parameter>& parameters) {
  if (!_param_client) {
    // LOG_WARN << "parameter client not found";
    return;
  }
  if (param_names.empty()) {
    _param_client->ListParameters(&parameters);
    return;
  }
  for (const auto& param_name : param_names) {
    cyber::Parameter parameter;
    auto nresult = _param_client->GetParameter(std::string(param_name), &parameter);
    if (nresult) {
      parameters.push_back(parameter);
    } else {
      LOG_WARN << "parameter: " << param_name << " not found";
    }
  }
}

bool CyberBridge::start() {
  // init node
  if (!_node) {
    _node = cyber::CreateNode("cyber_bridge");
    LOG_INFO << "create node: " << _node->Name();
  }
  LOG_INFO << "start cyber bridge";
  return true;
}

void CyberBridge::stop() {
  _timer->Stop();
  _node->ClearData();
  LOG_INFO << "stop cyber bridge";
}
