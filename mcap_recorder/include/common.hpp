#pragma once

#include <cyber/proto/proto_desc.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>

#include <queue>
using namespace apollo;
// using namespace gwm::adcos;

// ======================
// Cyber string → MCAP string
// ======================
static inline std::string CyberProtoDescStringToFdSetString(
  const std::string& cyber_proto_desc_str) {
  cyber::proto::ProtoDesc root;
  if (!root.ParseFromString(cyber_proto_desc_str)) {
    return "";  // 解析失败返回空串
  }

  google::protobuf::FileDescriptorSet fdSet;
  std::unordered_set<std::string> seen;
  std::queue<const cyber::proto::ProtoDesc*> q;
  q.push(&root);

  while (!q.empty()) {
    const auto* current = q.front();
    q.pop();

    google::protobuf::FileDescriptorProto file_desc_proto;
    if (!file_desc_proto.ParseFromString(current->desc())) {
      continue;
    }

    if (seen.insert(file_desc_proto.name()).second) {
      *fdSet.add_file() = file_desc_proto;
    }

    for (const auto& dep : current->dependencies()) {
      q.push(&dep);
    }
  }

  std::string out;
  fdSet.SerializeToString(&out);
  return out;
}

// ======================
// MCAP string → Cyber string
// ======================
static inline std::string FdSetStringToCyberProtoDescString(const std::string& fd_set_str) {
  google::protobuf::FileDescriptorSet fdSet;
  if (!fdSet.ParseFromString(fd_set_str)) {
    return "";  // 无效输入
  }

  if (fdSet.file_size() == 0) {
    return "";
  }

  std::unordered_map<std::string, const google::protobuf::FileDescriptorProto*> fileMap;
  std::unordered_set<std::string> allDeps;
  for (int i = 0; i < fdSet.file_size(); ++i) {
    fileMap[fdSet.file(i).name()] = &fdSet.file(i);
    for (int j = 0; j < fdSet.file(i).dependency_size(); ++j) {
      allDeps.insert(fdSet.file(i).dependency(j));
    }
  }

  std::vector<const google::protobuf::FileDescriptorProto*> roots;
  for (const auto& file : fdSet.file()) {
    if (allDeps.find(file.name()) == allDeps.end()) {
      roots.push_back(&file);
    }
  }

  std::function<void(const google::protobuf::FileDescriptorProto&, cyber::proto::ProtoDesc*)>
    buildTree;
  buildTree = [&](const google::protobuf::FileDescriptorProto& file,
                cyber::proto::ProtoDesc* descNode) {
    std::string str;
    file.SerializeToString(&str);
    descNode->set_desc(str);

    for (int i = 0; i < file.dependency_size(); ++i) {
      const std::string& depName = file.dependency(i);
      auto it = fileMap.find(depName);
      if (it != fileMap.end()) {
        auto* depNode = descNode->add_dependencies();
        buildTree(*it->second, depNode);
      }
    }
  };

  cyber::proto::ProtoDesc rootNode;
  if (!roots.empty()) {
    buildTree(*roots.front(), &rootNode);
  } else {
    buildTree(fdSet.file(0), &rootNode);
  }

  std::string out;
  rootNode.SerializeToString(&out);
  return out;
}

// ======================
// Cyber message → MCAP message
// ======================
