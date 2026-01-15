# Foxglove Record Data Loader 测试指南

本文档描述了如何运行和扩展Foxglove Record数据加载器的测试。

## 测试结构

项目包含两种类型的测试：

1. **Rust单元测试** - 测试核心数据结构和解析逻辑
2. **JavaScript集成测试** - 测试插件注册和文件读取功能

## 运行测试

### 运行所有测试
```bash
npm test
```

### 只运行Rust测试
```bash
npm run test:rust
```

### 只运行JavaScript测试
```bash
npm run test:js
```

## 测试详情

### Rust单元测试 (`rust/src/lib.rs`)

测试内容包括：

- **数据结构测试**：验证Header、Channel、Message等数据结构的编码/解码
- **枚举测试**：验证SectionType和CompressType枚举值
- **加载器测试**：测试RecordDataLoader的创建和基本功能
- **迭代器测试**：测试RecordMessageIterator的空状态

### JavaScript集成测试 (`test/test_loader.js`)

测试内容包括：

- **插件注册测试**：验证插件在Foxglove环境中的注册过程
- **文件结构测试**：创建和验证测试用的record文件数据
- **文件读取测试**：模拟文件读取操作

## 添加新测试

### 添加Rust测试

在`rust/src/lib.rs`的`tests`模块中添加新的测试函数：

```rust
#[test]
fn test_your_feature() {
    // 测试代码
    assert!(true);
}
```

### 添加JavaScript测试

在`test/test_loader.js`中添加新的测试用例：

```javascript
// 测试新的功能
console.log('4. 测试新功能...');
// 测试代码
console.log('✓ 新功能测试通过');
```

## 测试数据

测试使用模拟的record文件数据，包括：

- **Header部分**：包含版本信息、时间范围等元数据
- **Channel部分**：定义数据通道信息
- **Message部分**：包含时间戳和内容数据

## 调试测试

### 查看详细输出
测试运行时会输出详细的日志信息，包括：
- 测试步骤描述
- 数据验证结果
- 错误信息（如果有）

### 调试Rust测试
```bash
cd rust
cargo test -- --nocapture  # 显示标准输出
```

### 调试JavaScript测试
```bash
node test/test_loader.js
```

## 测试覆盖率

目前测试覆盖了以下核心功能：

- ✅ 数据结构序列化/反序列化
- ✅ 插件注册机制
- ✅ 文件读取操作
- ✅ 枚举值验证
- ✅ 数据加载器创建

## 扩展测试

要扩展测试覆盖范围，可以考虑添加：

1. **性能测试**：测试大数据文件的处理性能
2. **错误处理测试**：测试各种错误情况的处理
3. **边界条件测试**：测试极端情况下的行为
4. **集成测试**：与真实Foxglove Studio环境的集成测试

## 故障排除

如果测试失败：

1. 检查依赖是否安装完整：`npm install`
2. 确保Rust工具链可用：`rustc --version`
3. 查看详细的错误信息输出
4. 验证测试数据格式是否正确
