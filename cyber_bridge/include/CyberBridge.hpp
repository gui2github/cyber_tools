#pragma once
#include <cyber/cyber.h>
#include <cyber/node/reader.h>
#include <cyber/parameter/parameter_client.h>
#include <cyber/parameter/parameter_server.h>
#include <cyber/time/rate.h>
#include <cyber/time/time.h>

#include <optional>
#include <queue>

using namespace apollo;
// using namespace gwm::adcos;
using MessageBase = cyber::message::RawMessage;
class mssageManage;  // manage message

struct Schema {
  std::string name;
  std::string desc;
};

class CyberBridge {
public:
  CyberBridge();
  ~CyberBridge();

  using adCallback = std::function<void(
    const std::string& topic, Schema& schema_1, std::optional<Schema>& schema_2)>;
  using msgCallback = std::function<void(const std::string& topic, const std::string& msg)>;
  using unScribeCallback = std::function<void(const std::string& topic)>;
  void startDiscoverTimer(const adCallback& topic_adCb, const unScribeCallback& topic_unadCb,
    const adCallback& service_adCb);
  void onUnsubscribe(const std::string& topic);
  void onSubscribe(const std::string& topic, const msgCallback& cb);
  void onWriterCreate(const std::string& topic, const std::string& msg_type);
  void onWriterDelete(const std::string& topic);
  void onReceiveMsg(const std::string& topic, const std::string& msg);

  bool start();
  void stop();

  std::vector<std::string> getTopics() const {
    return _topics;
  }
  std::shared_ptr<mssageManage> getMsgManage(const std::string& topic) {
    return _msg_manages[topic];
  }
  // std::weak_ptr<Reader<MessageBase>> getReader(const std::string& topic)
  // {
  //     return _readers[topic];
  // }
  void onServiceRegister(const std::string& service_name);
  void onServiceCall(
    const std::shared_ptr<MessageBase>& request, std::shared_ptr<MessageBase>& response);
  void onServiceUnregister(const std::string& service_name);

  void onClientRegister(const std::string& client_name);
  void onClientCall(const std::string& topic, const std::string& req, std::string& res);
  void onClientUnregister(const std::string& client_name);

  void onSetParameter(std::string& key, std::string& type, cyber::Parameter& parameter);
  void onGetParameter(
    const std::vector<std::string_view>& param_names, std::vector<cyber::Parameter>& parameters);

private:
  void discoverTopics(const adCallback& adCb, const unScribeCallback& unScribeCb);
  void discoverServices(const adCallback& adCb);

private:
  std::vector<std::string> _topics;
  std::shared_ptr<cyber::Timer> _timer;
  std::shared_ptr<cyber::Node> _node;
  // std::shared_ptr<cyber::ParameterServer> _param_server;
  std::shared_ptr<cyber::ParameterClient> _param_client;

  std::map<std::string, std::shared_ptr<mssageManage>> _msg_manages;
  std::map<std::string, std::shared_ptr<cyber::ReaderBase>> _readers;
  std::map<std::string, std::shared_ptr<cyber::WriterBase>> _writers;
  std::map<std::string, std::shared_ptr<mssageManage>> _srv_msgs;
  std::map<std::string, std::pair<std::shared_ptr<mssageManage>, std::shared_ptr<mssageManage>>>
    _services;
  std::map<std::string, std::shared_ptr<cyber::ClientBase>> _clients;
};
