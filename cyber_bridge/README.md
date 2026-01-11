# Foxglove Bridge

一个用于将 Cyber 消息系统桥接到 Foxglove Studio 的工具，支持实时数据可视化和分析。

## 功能特性

- 🚀 **实时数据桥接**: 将 Cyber 消息系统数据实时传输到 Foxglove Studio
- 🔄 **双向通信**: 支持消息订阅和发布
- 📊 **数据可视化**: 在 Foxglove Studio 中可视化各种传感器数据
- 🔧 **自定义转换**: 支持自定义 Protobuf 消息到 Foxglove 格式的转换
- 🛠️ **服务支持**: 支持 Cyber 服务调用
- 💾 **数据记录**: 支持 MCAP 格式数据记录

## 系统要求

### 最低要求
- **Foxglove Studio**: 2.30 或更高版本 ([更新日志](https://docs.foxglove.dev/changelog))
- **Foxglove SDK**: 0.14.1
- **ADCOS**: 2.2.3 或更高版本
- **平台**: x86 架构

### 依赖项
- `adc_proto` - 可替换为本地版本
- `adf_proto` - 可替换为本地版本
- `protobuf` 3.14.0
- `foxglove_cpp` SDK

## 快速开始

### 1. 构建项目

```bash
# 默认构建 (x86平台)
./build.sh

# 指定平台和构建类型
./build.sh -p x86 -t RelWithDebInfo
```

构建选项:
- `-p, --platform`: 平台 (x86, orinx, thor)
- `-t, --type`: 构建类型 (Debug, Release, RelWithDebInfo, MinSizeRel)
- `-i, --interface`: 使用接口编译

### 2. 运行程序

```bash
# 使用默认配置
./scripts/run.sh

# 指定 IP 和端口
./scripts/run.sh -i 192.168.1.100 -p 8765
```

### 3. 连接 Foxglove Studio

1. 打开 Foxglove Studio
2. 选择 "Foxglove WebSocket" 连接类型
3. 输入服务器地址和端口 (例如: `ws://192.168.1.100:8765`)
4. 连接成功后即可查看实时数据

## 配置说明

### 环境变量

```bash
# 日志级别
export HAVP_LOG_LEVEL=INFO  # DEBUG, VERBOSE, INFO, WARNING, ERROR, FATAL

# 日志输出类型
export HAVP_LOG_OUTPUT_TYPE=screen  # screen, file, both

# 日志路径
export HAVP_LOG_PATH=/data/log/$(date +"%Y%m%d_%H%M%S")
```

### 命令行参数

```bash
fox_bridge --fox_addr=192.168.1.100 --fox_port=8765
```

- `--fox_addr`: Foxglove 服务器地址 (默认: 127.0.0.1)
- `--fox_port`: Foxglove 服务器端口 (默认: 8765)

## 自定义消息转换

### 添加自定义转换函数

如果需要将自定义 Protobuf 消息转换为 Foxglove 支持的消息格式，可以按照以下模板编写转换函数：

```cpp
#include "common/math/geometry.pb.h"
#include "foxglove/Quaternion.pb.h"
#include "logger/Log.h"

using foxglove::Quaternion;
using gwm::common::Orientation;

static void eulerToQuaternion(const Orientation& euler, Quaternion& q) {
  // 计算每个角的一半
  double cy = cos(euler.roll() * 0.5);  // yaw: Z
  double sy = sin(euler.yaw() * 0.5);
  double cp = cos(euler.pitch() * 0.5);  // pitch: Y
  double sp = sin(euler.pitch() * 0.5);
  double cr = cos(euler.roll() * 0.5);  // roll: X
  double sr = sin(euler.roll() * 0.5);
  q.set_w(cr * cp * cy + sr * sp * sy);
  q.set_x(sr * cp * cy - cr * sp * sy);
  q.set_y(cr * sp * cy + sr * cp * sy);
  q.set_z(cr * cp * sy - sr * sp * cy);
}

#include "foxglove/FrameTransform.pb.h"
#include "localization/gwm_localization_dr_out.pb.h"

using foxglove::FrameTransform;
using gwm::localization::DrOut;

// 示例转换函数实现
static void convertDR2Foxglove(const std::string& input, std::string& out) {
  DrOut drOut;
  drOut.ParseFromString(input);
  FrameTransform tf;
  tf.set_child_frame_id("vehicle");
  tf.set_parent_frame_id("dr");
  eulerToQuaternion(drOut.orientation(), *tf.mutable_rotation());
  tf.mutable_translation()->set_x(drOut.position().x());
  tf.mutable_translation()->set_y(drOut.position().y());
  tf.mutable_translation()->set_z(drOut.position().z());
  tf.mutable_timestamp()->set_seconds(drOut.sensor_time() / 1000000000);
  tf.mutable_timestamp()->set_nanos(drOut.sensor_time() % 1000000000);
  tf.SerializeToString(&out);
}
```

将编写好的转换函数发送给开发人员添加到系统中。

## 服务支持

### 服务发现机制

当前 Cyber Service 无服务发现机制，如需使用服务功能，请将以下信息发送给开发人员添加：

```cpp
{"/gwm/sm/parking_server",
 {"gwm.sm.parking.ParkingSMInfo", "gwm.sm.parking.ParkingSMResponse"}}
```

格式说明:
- `channel_name`: 服务通道名称
- `request_type`: 请求消息类型
- `response_type`: 响应消息类型

## 项目结构

```
foxglove_bridge/
├── 3dparty/          # 第三方依赖
│   └── foxglove/     # Foxglove SDK
├── include/          # 头文件
│   ├── CyberBridge.hpp
│   ├── FastDDSBridge.hpp
│   ├── FoxgloveServer.hpp
│   ├── MessageConverter.hpp
│   └── service_impl.hpp
├── src/             # 源代码
│   ├── main.cpp
│   ├── CyberBridge.cpp
│   ├── FastDDSBridge.cpp
│   ├── FoxgloveServer.cpp
│   └── MessageConverter.cpp
├── scripts/         # 脚本文件
│   └── run.sh
├── test/           # 测试代码
└── toolchains/     # 构建工具链
```

## 核心组件

### FoxgloveServer
- WebSocket 服务器实现
- 消息通道管理
- 参数存储和管理

### CyberBridge
- Cyber 消息系统桥接
- 主题发现和订阅
- 服务调用支持

### MessageConverter
- 消息格式转换
- 自定义转换器注册
- Protobuf 描述符管理

## 故障排除

### 常见问题

1. **连接失败**
   - 检查网络连接和防火墙设置
   - 确认 Foxglove Studio 版本符合要求

2. **消息不显示**
   - 检查消息类型是否支持
   - 确认转换函数是否正确注册

3. **服务调用失败**
   - 确认服务已正确注册
   - 检查请求/响应消息格式

### 日志查看

```bash
# 设置详细日志
export HAVP_LOG_LEVEL=DEBUG
export HAVP_LOG_OUTPUT_TYPE=both
```

## 免责声明

- 本软件仅对最新 ADCOS 提供支持
- 仅对 x86 平台提供支持
- 其他涉及网络配置、环境配置等报错的概不负责，请自行解决

## 技术支持

如有问题或需要添加新功能，请联系开发团队并提供：
1. 错误日志
2. 复现步骤
3. 相关配置信息

## 许可证

[在此添加许可证信息]
