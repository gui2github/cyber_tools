#include <cyber/message/protobuf_factory.h>
#include <cyber/record/record_writer.h>
#include <logger/log.h>

#include <mcap/mcap.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "common.hpp"
#include "mcap_to_cyber_converter.h"

// using namespace gwm::adcos;

bool McapToCyberConverter::convert(const std::string& inputFile, const std::string& outputFile) {
  // 打开mcap文件
  auto reader = std::make_shared<mcap::McapReader>();
  auto status = reader->open(inputFile);
  if (!status.ok()) {
    LOG_ERROR << "Failed to open mcap file: " << status.message;
    return false;
  }

  // 创建cyber record writer
  auto writer = std::make_shared<cyber::record::RecordWriter>();
  if (!writer->Open(outputFile)) {
    LOG_ERROR << "Failed to open cyber record file: " << outputFile;
    return false;
  }
  LOG_INFO << "Converting mcap to cyber record...";
  // 读取summary
  status = reader->readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
  if (!status.ok()) {
    LOG_ERROR << "Failed to read mcap summary: " << status.message;
    return false;
  }
  std::unordered_set<std::string> registeredChannels;
  // 获取channels信息
  auto channels = reader->channels();
  auto schemas = reader->schemas();
  // static const auto cyber_factory = cyber::message::ProtobufFactory::Instance();
  // 在cyber record中注册channels
  for (const auto& [channelId, channel] : channels) {
    auto schema = schemas[channel->schemaId];
    if (!schema) {
      LOG_WARN << "No schema found for channel: " << channel->topic;
      continue;
    }
    if (schema->encoding != "protobuf") {
      LOG_WARN << "Unsupported encoding: " << schema->encoding;
      continue;
    }
    std::string protoDesc(reinterpret_cast<const char*>(schema->data.data()), schema->data.size());
    std::string proto_desc_str = FdSetStringToCyberProtoDescString(protoDesc);
    if (proto_desc_str.empty()) {
      LOG_WARN << "Failed to convert proto desc to fd set string";
      continue;
    }
    if (!writer->WriteChannel(channel->topic, schema->name, proto_desc_str)) {
      LOG_WARN << "Failed to register message type: " << schema->name;
      continue;
    }
    registeredChannels.insert(channel->topic);
    LOG_DEBUG << "Registered channel: " << channel->topic << " (msg_type: " << schema->name << ")";
  }

  // 读取并转换消息
  uint64_t messageCount = 0;
  auto startTime = std::chrono::steady_clock::now();
  for (const auto& msgView : reader->readMessages()) {
    auto channel = reader->channel(msgView.message.channelId);
    if (!channel) {
      LOG_WARN << "No channel found for message with channelId: " << msgView.message.channelId;
      continue;
    }
    if (registeredChannels.find(channel->topic) == registeredChannels.end()) {
      // LOG_WARN << "Skipping message from unknown channel: " << channel->topic;
      continue;
    }
    std::string content(
      reinterpret_cast<const char*>(msgView.message.data), msgView.message.dataSize);
    writer->WriteMessage(channel->topic, content, msgView.message.logTime);
    messageCount++;
  }
  auto endTime = std::chrono::steady_clock::now();
  writer->Close();
  reader->close();
  LOG_INFO << "Conversion completed. Total messages: " << messageCount << ". Time taken: "
       << std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count()
       << " seconds.";
  return true;
}
