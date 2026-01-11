#pragma once
#include <cyber/cyber.h>
#include <cyber/message/protobuf_factory.h>
// #include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <logger/log.h>

#include <nlohmann/json.hpp>
#include <queue>

using json = nlohmann::json;

inline static std::string SerializeFdSet(const google::protobuf::Descriptor* descriptor) {
  if (!descriptor) {
    return "";
  }
  google::protobuf::FileDescriptorSet fdSet;
  std::queue<const google::protobuf::FileDescriptor*> toAdd;
  std::unordered_set<std::string> seenDependencies;
  toAdd.push(descriptor->file());
  seenDependencies.insert(descriptor->file()->name());
  while (!toAdd.empty()) {
    const google::protobuf::FileDescriptor* next = toAdd.front();
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
// using namespace gwm::adcos;
using namespace apollo;
using MessageBase = cyber::message::RawMessage;

class mssageManage {
public:
  mssageManage()
      : raw_msg_class_(nullptr)
      , descriptor_(nullptr) {}

  ~mssageManage() = default;

  bool init_topic(const std::string& topic) {
    if (topic.empty()) {
      LOG_WARN << "Topic is empty";
      return false;
    }
    auto topology = cyber::service_discovery::TopologyManager::Instance();
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    auto channel_manager = topology->channel_manager();
    std::string msg_type;
    channel_manager->GetMsgType(topic, &msg_type);
    return init_type(msg_type);
  }

  bool init_type(const std::string& msg_type) {
    if (msg_type.empty()) {
      LOG_WARN << "msg_type is empty";
      return false;
    }
    if (!createMessageInstance(msg_type)) {
      LOG_WARN << "Failed to create message instance: " << msg_type;
      return false;
    }
    LOG_INFO << "msg_type: " << msg_type;
    return true;
  }

  // 将消息转换为 Protobuf 字符串
  std::string getMsgProtoString(const std::shared_ptr<MessageBase> raw_msg) {
    raw_msg_class_->ParseFromString(raw_msg->message);
    return raw_msg_class_->SerializeAsString();
  }

  // 将消息转换为 JSON 字符串
  std::string getMsgJsonString(const std::shared_ptr<MessageBase> raw_msg) {
    raw_msg_class_->ParseFromString(raw_msg->message);
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = false;
    options.preserve_proto_field_names = false;
    options.always_print_enums_as_ints = true;
    std::string json_string;
    auto status =
      google::protobuf::util::MessageToJsonString(*raw_msg_class_, &json_string, options);
    if (!status.ok()) {
      LOG_WARN << "Failed to convert message to JSON: " << status.error_message();
      return "";
    }
    return json_string;
  }

  // 从 JSON 字符串创建消息
  bool getMsgFromJsonString(
    const std::string& msg_json_string, std::shared_ptr<MessageBase>& raw_msg) {
    auto status =
      google::protobuf::util::JsonStringToMessage(msg_json_string, raw_msg_class_.get());
    if (!status.ok()) {
      LOG_WARN << "Failed to convert JSON to message: " << status.error_message();
      return false;
    }
    if (!raw_msg_class_->SerializeToString(&raw_msg->message)) {
      LOG_WARN << "Failed to serialize message";
      return false;
    }
    return true;
  }

  // 获取消息类型
  const std::string getType() const {
    return descriptor_ ? descriptor_->full_name() : "";
  }

  // 获取文件描述符集合
  const std::string getFdSet() const {
    return SerializeFdSet(descriptor_);
  }

  // 获取 JSON Schema
  const std::string getJsonSchema() {
    auto schema = buildJsonSchemaFromDescriptor(descriptor_);
    return schema.dump();
  }

  // 获取消息描述符
  const auto getDescriptor() const {
    return descriptor_;
  }

private:
  // 使用 Protobuf 原生机制查找消息描述符
  bool createMessageInstance(const std::string& msg_type) {
    // 先尝试使用 cyber 机制查找消息描述符
    static const auto cyber_factory = cyber::message::ProtobufFactory::Instance();
    descriptor_ = cyber_factory->FindMessageTypeByName(msg_type);
    raw_msg_class_ =
      std::unique_ptr<google::protobuf::Message>(cyber_factory->GenerateMessageByType(msg_type));
    if (descriptor_ && raw_msg_class_) {
      return true;
    }
    // 如果 cyber 机制查找失败，则使用 Protobuf 原生机制查找消息描述符
    static auto pool = google::protobuf::DescriptorPool::generated_pool();
    descriptor_ = pool->FindMessageTypeByName(msg_type);
    raw_msg_class_ = createMessageInstance(descriptor_);
    if (!descriptor_ || !raw_msg_class_) {
      LOG_WARN << "Failed to find descriptor for msg_type: " << msg_type;
      return false;
    }
    cyber_factory->RegisterMessage(*raw_msg_class_.get());  // 注册消息描述符到 cyber
    return true;
  }

  // 创建消息实例
  std::unique_ptr<google::protobuf::Message> createMessageInstance(
    const google::protobuf::Descriptor* descriptor) {
    if (!descriptor) {
      return nullptr;
    }
    // 尝试使用静态工厂创建消息实例
    static const auto factory = google::protobuf::MessageFactory::generated_factory();
    const google::protobuf::Message* prototype = factory->GetPrototype(descriptor);
    if (!prototype) {
      // 尝试使用动态工厂创建消息实例
      static const auto dynamic_factory =
        std::make_unique<google::protobuf::DynamicMessageFactory>();
      prototype = dynamic_factory->GetPrototype(descriptor);
      if (!prototype) {
        return nullptr;
      }
    }
    return std::unique_ptr<google::protobuf::Message>(prototype->New());
  }

  // 获取 JSON Schema（通过 descriptor）
  const json buildJsonSchemaFromDescriptor(const google::protobuf::Descriptor* descriptor) {
    if (!descriptor) {
      return json{};
    }

    json schema;
    schema["type"] = "object";
    json properties = json::object();

    for (int i = 0; i < descriptor->field_count(); i++) {
      const google::protobuf::FieldDescriptor* field = descriptor->field(i);
      json fieldSchema;

      switch (field->type()) {
        case google::protobuf::FieldDescriptor::TYPE_INT32:
        case google::protobuf::FieldDescriptor::TYPE_INT64:
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
        case google::protobuf::FieldDescriptor::TYPE_SINT32:
        case google::protobuf::FieldDescriptor::TYPE_SINT64:
        case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
          fieldSchema["type"] = "integer";
          break;

        case google::protobuf::FieldDescriptor::TYPE_FLOAT:
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
          fieldSchema["type"] = "number";
          break;

        case google::protobuf::FieldDescriptor::TYPE_BOOL:
          fieldSchema["type"] = "boolean";
          break;

        case google::protobuf::FieldDescriptor::TYPE_STRING:
        case google::protobuf::FieldDescriptor::TYPE_BYTES:
          fieldSchema["type"] = "string";
          break;

        case google::protobuf::FieldDescriptor::TYPE_ENUM: {
          fieldSchema["type"] = "string";
          json enumValues = json::array();
          const auto* enumDesc = field->enum_type();
          for (int j = 0; j < enumDesc->value_count(); j++) {
            enumValues.push_back(enumDesc->value(j)->name());
          }
          fieldSchema["enum"] = enumValues;
          break;
        }

        case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
          // 递归构建嵌套 message
          fieldSchema = buildJsonSchemaFromDescriptor(field->message_type());
          break;
        }

        default:
          fieldSchema["type"] = "string";  // 兜底
          break;
      }

      if (field->is_repeated()) {
        json arrSchema;
        arrSchema["type"] = "array";
        arrSchema["items"] = fieldSchema;
        properties[field->name()] = arrSchema;
      } else {
        properties[field->name()] = fieldSchema;
      }
    }
    schema["properties"] = properties;
    return schema;
  }

private:
  std::unique_ptr<google::protobuf::Message> raw_msg_class_;  // 用于存储解析后的消息
  const google::protobuf::Descriptor* descriptor_;
};
