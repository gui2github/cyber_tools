# Cyber Data Loader - Foxglove 扩展

这是一个基于 Rust 和 WebAssembly 的 Foxglove 数据加载器扩展，用于解析和加载 `.record` 格式的数据文件。

## 项目概述

- **名称**: cyber-data-loader
- **类型**: Foxglove 数据加载器扩展
- **支持的文件格式**: `.record`
- **技术栈**: Rust + WebAssembly + TypeScript

## 开发环境要求

### 系统要求
- Node.js 16+
- Rust 工具链 (stable)
- wasm32-unknown-unknown 目标

### 安装依赖

```bash
# 安装 Node.js 依赖
npm install

# 安装 Rust 工具链 (如果尚未安装)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup target add wasm32-unknown-unknown
```

## 编译流程

### 1. Protobuf 代码生成

项目使用 Protobuf 定义数据结构，需要从 `.proto` 文件生成 Rust 代码：

```bash
cd rust

# 如果使用 prost-build，需要在 build.rs 中配置
# 或者手动生成：
cargo install prost-build

# 生成 Rust 代码
prost-build --out-dir src record.proto
```

**注意**: 当前项目中 `record.rs` 文件已经预先生成，如果需要修改数据结构：

1. 编辑 `rust/src/record.proto` 文件
2. 运行上述生成命令
3. 生成的代码将保存在 `rust/src/record.rs`

### 2. 构建 WebAssembly 模块

```bash
# 构建 WASM 模块 (发布模式)
npm run build:wasm

# 或者直接使用 cargo
cd rust
cargo build --release --target wasm32-unknown-unknown
```

### 2. 构建 TypeScript 扩展

```bash
# 构建完整的扩展
npm run build

# 开发模式构建
foxglove-extension build
```

### 3. 本地安装和测试

```bash
# 本地安装到 Foxglove 桌面应用
npm run local-install

# 安装后，打开 Foxglove 桌面应用或按 Ctrl+R 刷新
# 扩展将出现在数据源列表中
```

### 4. 打包扩展

```bash
# 打包为 .foxe 文件
npm run package

# 生成的文件: gli.cyber-data-loader-0.1.0.foxe
```

## 项目结构

```
cyber-data-loader/
├── rust/                    # Rust WASM 代码
│   ├── src/
│   │   ├── lib.rs          # 主要 Rust 实现
│   │   ├── record.rs       # Protobuf 生成的数据结构
│   │   ├── record.proto    # Protobuf 定义文件
│   │   └── test_utils.rs   # 测试工具函数
│   └── Cargo.toml          # Rust 依赖配置
├── src/
│   ├── index.ts            # TypeScript 扩展入口
│   └── globals.d.ts        # 类型定义
├── test/                   # 测试文件
│   ├── simple_test.js      # JavaScript 集成测试
│   └── test_loader.js      # 测试工具
├── dist/                   # 构建输出目录
└── package.json            # Node.js 配置
```

## 数据格式支持

### Record 文件结构
扩展支持解析以下 record 文件结构：

- **Header**: 文件元数据（版本、时间范围、压缩类型等）
- **ChunkHeader**: 数据块头部信息
- **ChunkBody**: 包含实际消息数据
- **Index**: 消息索引信息
- **Channel**: 数据通道定义

### 支持的消息类型
- 时间戳数据
- 多通道数据流
- 压缩数据（BZ2、LZ4、ZSTD）

## 调试和测试

### 运行测试

```bash
# 运行所有测试
npm run test:rust && npm run test:js

# 只运行 Rust 测试
npm run test:rust

# 只运行 JavaScript 测试
npm run test:js
```

### 调试日志

扩展包含详细的调试日志，可以在 Foxglove Studio 开发者工具中查看：

1. 按 F12 打开开发者工具
2. 切换到 Console 标签页
3. 查看插件激活和文件解析日志

### 常见问题排查

**问题**: "Unsupported extension: '.record'"
**解决方案**:
- 确认插件已正确安装
- 检查控制台日志中的插件注册信息
- 验证 WASM 文件是否正确加载

## 发布流程

### 1. 更新版本号

在 `package.json` 中更新版本号：

```json
{
  "version": "0.1.0",
  "publisher": "your-publisher-id",
  "description": "Your extension description"
}
```

### 2. 打包发布

```bash
npm run package
```

### 3. 发布到扩展注册表

参考 Foxglove 官方文档：
https://docs.foxglove.dev/docs/visualization/extensions/publish/

## 开发指南

### Protobuf 开发流程

#### 修改数据结构
1. 编辑 `rust/src/record.proto` 文件
2. 重新生成 Rust 代码：
   ```bash
   cd rust
   # 如果使用 prost-build 工具
   prost-build --out-dir src record.proto
   # 或者直接运行构建，如果配置了 build.rs
   cargo build
   ```
3. 检查生成的 `rust/src/record.rs` 文件
4. 在 `rust/src/lib.rs` 中更新使用新的数据结构

#### Protobuf 定义示例
```protobuf
syntax = "proto3";

message Header {
  uint32 major_version = 1;
  uint32 minor_version = 2;
  // ... 其他字段
}
```

### 添加新的数据格式支持

1. 在 `rust/src/record.proto` 中添加新的 Protobuf 定义
2. 重新生成 Rust 代码（使用上述流程）
3. 在 `rust/src/lib.rs` 中实现新的解析逻辑
4. 添加相应的测试用例

### 扩展 Foxglove 模式支持

当前使用 `foxglove::schemas::RawAudio` 作为通用模式，可以根据实际数据类型替换为：
- `foxglove::schemas::PointCloud`
- `foxglove::schemas::Image`
- `foxglove::schemas::LaserScan`
- 或其他合适的模式

## 贡献指南

1. Fork 项目
2. 创建功能分支
3. 提交更改
4. 运行测试确保通过
5. 创建 Pull Request

## 许可证

本项目采用 UNLICENSED 许可证。

## 相关文档

- [Foxglove 扩展开发文档](https://docs.foxglove.dev/docs/visualization/extensions/introduction)
- [Rust WebAssembly 指南](https://rustwasm.github.io/docs/book/)
- [Protobuf Rust 指南](https://docs.rs/prost/latest/prost/)
- [prost-build 工具文档](https://docs.rs/prost-build/latest/prost_build/)
- [Protocol Buffers 语言指南](https://developers.google.com/protocol-buffers/docs/proto3)
