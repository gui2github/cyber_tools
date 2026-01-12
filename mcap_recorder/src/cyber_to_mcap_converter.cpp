#include "cyber_to_mcap_converter.h"

#include <cyber/message/protobuf_factory.h>
#include <cyber/record/record_reader.h>
#include <cyber/record/record_viewer.h>
#include <logger/log.h>

#include <chrono>
#include <mcap/mcap.hpp>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "common.hpp"

// using namespace gwm::adcos;

bool CyberToMcapConverter::convert(const std::string& inputFile, const std::string& outputFile) {
  // 创建 Cyber record reader
  auto reader = std::make_shared<cyber::record::RecordReader>(inputFile);
  if (!reader->IsValid()) {
    LOG_ERROR << "Failed to open cyber record file: " << inputFile;
    return false;
  }
  auto header = reader->GetHeader();
//   auto adcos_version = header.adcos_version();
  auto compress_type = header.compress();
//   LOG_DEBUG << "adcos_version: " << adcos_version << ", compress_type: " << compress_type;
  // 创建 MCAP writer
  auto writer = std::make_shared<mcap::McapWriter>();
  mcap::McapWriterOptions options("");
  options.compression = mcap::Compression::Zstd;
  auto status = writer->open(outputFile, options);
  if (!status.ok()) {
    LOG_ERROR << "Failed to open mcap file: " << status.message;
    return false;
  }
  LOG_DEBUG << "Converting cyber record to mcap...";
  // 获取所有通道信息
  auto channelNames = reader->GetChannelList();
  LOG_DEBUG << "Total channels in record file: " << channelNames.size();

  std::unordered_map<std::string, mcap::SchemaId> schemaMap;
  // 添加 schema 和 channel
  // static const auto cyber_factory = cyber::message::ProtobufFactory::Instance();
  // std::this_thread::sleep_for(std::chrono::seconds(2));
  for (const auto& channelName : channelNames) {
    const std::string& messageType = reader->GetMessageType(channelName);
    std::string proto_desc = reader->GetProtoDesc(channelName);
    std::string mcap_desc = CyberProtoDescStringToFdSetString(proto_desc);
    mcap::Schema schema(messageType, "protobuf", mcap_desc);
    writer->addSchema(schema);
    mcap::Channel channel(channelName, "protobuf", schema.id);
    writer->addChannel(channel);
    schemaMap[channelName] = schema.id;
  }
  // 读取并转换消息
  uint64_t messageCount = 0;
  auto startTime = std::chrono::steady_clock::now();
  auto messageView = cyber::record::RecordViewer(reader);
  for (auto message = messageView.begin(); message != messageView.end(); ++message) {
    if (schemaMap.find(message->channel_name) == schemaMap.end()) {
      LOG_WARN << "Skipping unknown channel: " << message->channel_name;
      continue;
    }
    // 创建 MCAP 消息
    mcap::Message mcapMessage;
    mcapMessage.channelId = schemaMap[message->channel_name];
    mcapMessage.sequence = 0;             // Cyber record 没有序列号概念
    mcapMessage.logTime = message->time;  // Cyber 没有logTime概念，使用publishTime代替
    mcapMessage.publishTime = message->time;
    mcapMessage.data = reinterpret_cast<const std::byte*>(message->content.data());
    mcapMessage.dataSize = message->content.size();
    auto writeStatus = writer->write(mcapMessage);
    if (!writeStatus.ok()) {
      LOG_WARN << "Failed to write message: " << writeStatus.message;
      continue;
    }
    messageCount++;
  }
  auto endTime = std::chrono::steady_clock::now();
  writer->close();
  LOG_INFO << "Conversion completed. Total messages: " << messageCount << ". Time taken: "
       << std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count()
       << " seconds.";
  return true;
}
