#pragma once
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>

#include <any>
#include <functional>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

struct ConverterInfo {
  std::function<void(const std::string&, std::string&)> converter;
  std::string target_type;
  std::string target_schema;
};

class MessageConverter {
public:
  static MessageConverter& instance() {
    static MessageConverter inst;
    return inst;
  }

  // 注册proto到foxglove的转换器
  template<typename ProtoMsg, typename FoxgloveSchema>
  void registerConverter(std::function<void(const ProtoMsg&, std::string&)> converter) {
    const auto* descriptor = ProtoMsg::descriptor();
    const auto* target_descriptor = FoxgloveSchema::descriptor();

    type_registry_[descriptor->full_name()] = {
      [converter](const std::string& proto_str, std::string& output) {
        ProtoMsg msg;
        if (!msg.ParseFromString(proto_str)) {
          throw std::runtime_error("Failed to parse protobuf");
        }
        converter(msg, output);
      },
      target_descriptor->full_name(),
      SerializeFdSet(target_descriptor)};
  }

  // 查询是否有转换器
  bool hasConverter(const std::string& msg_type) const {
    return type_registry_.find(msg_type) != type_registry_.end();
  }

  // 执行转换
  void convert(
    const std::string& proto_str, const std::string& msg_type, std::string& output) const {
    auto it = type_registry_.find(msg_type);
    if (it == type_registry_.end()) {
      throw std::runtime_error("No converter registered for message type: " + msg_type);
    }
    it->second.converter(proto_str, output);
  }

  // 获取目标类型的Descriptor string
  std::string getTargetDescriptorString(const std::string& msg_type) const {
    return type_registry_.at(msg_type).target_schema;
  }

  // 获取目标类型名称
  std::string getTargetTypeName(const std::string& msg_type) const {
    return type_registry_.at(msg_type).target_type;
  }

private:
  std::unordered_map<std::string, ConverterInfo> type_registry_;

  using Descriptor = google::protobuf::Descriptor;
  using FileDescriptorSet = google::protobuf::FileDescriptorSet;
  using FileDescriptor = google::protobuf::FileDescriptor;

  static std::string SerializeFdSet(const Descriptor* descriptor) {
    google::protobuf::FileDescriptorSet fdSet;
    std::queue<const FileDescriptor*> toAdd;
    std::unordered_set<std::string> seenDependencies;
    toAdd.push(descriptor->file());
    seenDependencies.insert(descriptor->file()->name());
    while (!toAdd.empty()) {
      const FileDescriptor* next = toAdd.front();
      toAdd.pop();
      next->CopyTo(fdSet.add_file());
      for (int i = 0; i < next->dependency_count(); ++i) {
        const auto& dep = next->dependency(i);
        if (seenDependencies.find(dep->name()) == seenDependencies.end()) {
          seenDependencies.insert(dep->name());
          toAdd.push(dep);
        }
      }
    }
    return fdSet.SerializeAsString();
  }
};

#define REGISTER_MESSAGE_CONVERTER(ProtoType, FoxgloveType, ConverterFunc) \
  namespace { \
  const bool _registered_##ProtoType##_##FoxgloveType = []() -> bool { \
    MessageConverter::instance().registerConverter<ProtoType, FoxgloveType>( \
      [](const ProtoType& msg, std::string& output) { \
        ConverterFunc(msg.SerializeAsString(), output); \
      }); \
    return true; \
  }(); \
  }
