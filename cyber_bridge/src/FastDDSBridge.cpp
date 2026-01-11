#include "FastDDSBridge.hpp"
#include "FoxgloveServer.hpp"
#include "MessageConverter.hpp"

// 允许创建和销毁 DomainParticipant 对象。
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
// 充当所有其他实体对象的容器，并充当发布者、订阅者和主题对象的工厂。
#include <fastdds/dds/domain/DomainParticipant.hpp>
// 为participant提供序列化、反序列化和获取特定数据类型的key的函数。
#include <fastdds/dds/topic/TypeSupport.hpp>
// 负责创建和配置 DataReader 的对象。
#include <fastdds/dds/subscriber/Subscriber.hpp>
// 负责实际接收数据的对象。它在应用程序中注册标识要读取的数据的主题(TopicDescription)，并访问订阅者接收到的数据。
#include <fastdds/dds/subscriber/DataReader.hpp>
// 分配给数据读取器的侦听器
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
// 定义 DataReader 的 QoS 的结构。
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
// “read”或“taken”的是每个SampleInfo附带的信息。
#include <fastdds/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastdds/rtps/common/InstanceHandle.h>
#include <fastdds/rtps/common/Locator.h>
#include <fastdds/rtps/common/Types.h>
#include <fastdds/rtps/participant/RTPSParticipant.h>
#include <logger/Log.h>

#include <fastdds/dds/builtin/topic/ParticipantBuiltinTopicData.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/TopicDescription.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <iostream>
#include <map>
#include <vector>

FastDDSBridge::FastDDSBridge()
    : _participant(nullptr)
    , _subscriber(nullptr)
    , _topic(nullptr)
    , _reader(nullptr)
    , _state(BridgeState::Stopped)
    , _server(nullptr)
    , _converter(nullptr)
    , _recorder(nullptr)
    , _topics() {}

FastDDSBridge::~FastDDSBridge() {
  if (_reader) {
    _subscriber->delete_datareader(_reader);
  }
  if (_topic) {
    _participant->delete_topic(_topic);
  }
  if (_subscriber) {
    _participant->delete_subscriber(_subscriber);
  }
  if (_participant) {
    fdds::DomainParticipantFactory::get_instance()->delete_participant(_participant);
  }
}

bool FastDDSBridge::discoverTopics() {
  if (!_participant) {
    return false;
  }

  // 仅记录当前topic
  if (_topic) {
    _topics[_topic->get_name()] = _topic->get_type_name();
    LOG_INFO << "Current topic: " << _topic->get_name() << " with type: " << _topic->get_type_name();
  }

  // 确保记录当前topic
  if (_topic && _topics.find(_topic->get_name()) == _topics.end()) {
    _topics[_topic->get_name()] = _topic->get_type_name();
  }

  return !_topics.empty();
}

bool FastDDSBridge::addTopic(const std::string& topicName, const std::string& typeName) {
  if (!_participant) {
    return false;
  }

  _topics[topicName] = typeName;
  LOG_INFO << "Added topic: " << topicName << " with type: " << typeName;
  return true;
}

bool FastDDSBridge::initialize(const std::string& ip, int port, int domainId,
  const std::string& topicName, const std::string& typeName) {
  // 创建DomainParticipant
  fdds::DomainParticipantQos participantQos;
  participantQos.wire_protocol().builtin.discovery_config.initial_announcements.count = 3;
  participantQos.wire_protocol().builtin.discovery_config.initial_announcements.period = {
    0, 100000000};

  // 使用默认配置

  _participant =
    fdds::DomainParticipantFactory::get_instance()->create_participant(domainId, participantQos);
  if (!_participant) {
    LOG_ERROR << "Failed to create DomainParticipant";
    return false;
  }

  // 创建Subscriber
  fdds::SubscriberQos subQos;
  _subscriber = _participant->create_subscriber(subQos, nullptr);
  if (!_subscriber) {
    LOG_ERROR << "Failed to create Subscriber";
    return false;
  }

  // 创建Topic
  fdds::TopicQos topicQos;
  _topic = _participant->create_topic(topicName, typeName, topicQos);
  if (!_topic) {
    LOG_ERROR << "Failed to create Topic";
    return false;
  }

  // 创建DataReader
  fdds::DataReaderQos readerQos;
  _reader = _subscriber->create_datareader(_topic, readerQos, this);
  if (!_reader) {
    LOG_ERROR << "Failed to create DataReader";
    return false;
  }

  if (!discoverTopics()) {
    LOG_ERROR << "Failed to discover topics";
    return false;
  }
  return true;
}

void FastDDSBridge::setFoxgloveServer(std::shared_ptr<FoxgloveServer> server) {
  _server = server;
}

void FastDDSBridge::setMessageConverter(std::shared_ptr<MessageConverter> converter) {
  _converter = converter;
}

void FastDDSBridge::setMcapRecorder(std::shared_ptr<McapRecorder> recorder) {
  _recorder = recorder;
}

bool FastDDSBridge::startAllComponents() {
  if (_state != BridgeState::Stopped) {
    return false;
  }

  _state = BridgeState::Starting;
  try {
    if (_server) _server->start("0.0.0.0", 8765);
    if (_recorder) _recorder->start("recording.mcap");

    _state = BridgeState::Running;
    return true;
  } catch (const std::exception& e) {
    _state = BridgeState::Error;
    _errorMessage = e.what();
    return false;
  }
}

void FastDDSBridge::stopAllComponents() {
  if (_state == BridgeState::Running || _state == BridgeState::Error) {
    if (_server) _server->stop();
    if (_recorder) _recorder->stop();
    _state = BridgeState::Stopped;
  }
}

void FastDDSBridge::on_data_available(fdds::DataReader* reader) {
  if (_state != BridgeState::Running) {
    return;
  }

  std::string sample;
  if (reader->take_next_sample(&sample, nullptr) == ReturnCode_t::RETCODE_OK) {
    // 获取当前topic名称
    auto topic = reader->get_topicdescription();
    std::string topicName = topic->get_name();

    // 优先通过MessageConverter转换后再发送
    if (_converter) {
      _converter->convertAndSend(topicName, sample);
    }
    // 如果没有转换器，直接发送原始数据
    else if (_server) {
      _server->sendMessage(
        topicName, reinterpret_cast<const std::byte*>(sample.data()), sample.size());
    }

    if (_recorder) {
      // TODO: 添加记录功能
    }
  }
}

void FastDDSBridge::on_subscription_matched(
  fdds::DataReader* reader, const fdds::SubscriptionMatchedStatus& info) {
  if (info.current_count_change == 1) {
    LOG_INFO << "Matched a publisher on topic";
  } else if (info.current_count_change == -1) {
    LOG_INFO << "Unmatched a publisher on topic";
  }
}
