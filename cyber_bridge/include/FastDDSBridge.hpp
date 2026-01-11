#ifndef FAST_DDS_BRIDGE_HPP
#define FAST_DDS_BRIDGE_HPP

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <functional>
#include <memory>

class FoxgloveServer;
class MessageConverter;
class McapRecorder;

namespace fdds = eprosima::fastdds::dds;
class FastDDSBridge : public fdds::DataReaderListener {
  enum class BridgeState { Stopped, Starting, Running, Error };

public:
  using DataCallback = std::function<void(const std::string&)>;

  FastDDSBridge();
  ~FastDDSBridge();

  bool initialize(const std::string& ip, int port, int domainId, const std::string& topicName,
    const std::string& typeName);
  bool discoverTopics();
  bool addTopic(const std::string& topicName, const std::string& typeName);
  bool startAllComponents();
  void stopAllComponents();

  // Component management
  void setFoxgloveServer(std::shared_ptr<FoxgloveServer> server);
  void setMessageConverter(std::shared_ptr<MessageConverter> converter);
  void setMcapRecorder(std::shared_ptr<McapRecorder> recorder);

  // DataReaderListener callbacks
  void on_data_available(fdds::DataReader* reader) override;
  void on_subscription_matched(
    fdds::DataReader* reader, const fdds::SubscriptionMatchedStatus& info) override;

private:
  fdds::DomainParticipant* _participant;
  fdds::Subscriber* _subscriber;
  fdds::Topic* _topic;
  fdds::DataReader* _reader;

  BridgeState _state;
  std::string _errorMessage;

  std::shared_ptr<FoxgloveServer> _server;
  std::shared_ptr<MessageConverter> _converter;
  std::shared_ptr<McapRecorder> _recorder;
  std::map<std::string, std::string> _topics;
};

#endif  // FAST_DDS_BRIDGE_HPP
