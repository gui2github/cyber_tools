# MCAP Recorder

一个高性能的Cyber RT消息录制、转换和播放工具，支持MCAP和Cyber record格式的互转以及实时播包功能。

<table><tr><td bgcolor=yellow>
当前消息类型支持protobuf类型，ros2msg、ros2idl、json类型均不支持。
yellow</td></tr></table>

mcap包信息查看请使用官方提供的 [cli工具](https://mcap.dev/guides/cli)

## 功能特性

### 录制功能
- ✅ **多线程录制**：使用独立线程处理消息写入，避免阻塞消息接收
- ✅ **消息完整性**：保证开始录制和断开时消息的完整性
- ✅ **自动发现机制**：动态发现新channel并自动加入录制
- ✅ **灵活的过滤策略**：支持白名单、黑名单过滤，默认录制所有channel
- ✅ **分段录制**：支持按时间间隔自动分段录制，时间戳一致便于管理
- ✅ **智能命名**：支持自定义文件名或自动使用时间戳命名
- ✅ **高性能**：使用RawMessage指针避免数据拷贝

### 格式转换功能
- ✅ **Cyber record → MCAP**：将Cyber record文件转换为MCAP格式
- ✅ **MCAP → Cyber record**：将MCAP文件转换为Cyber record格式
- ✅ **自动检测**：根据文件扩展名自动选择转换方向

### 播包功能
- ✅ **实时播包**：通过Cyber pub功能播放MCAP文件
- ✅ **速度控制**：支持播放速度调节（支持快进和慢放）
- ✅ **起始时间**：支持从指定秒数开始播放，跳过前面的内容
- ✅ **暂停/恢复**：按空格键暂停和恢复播放
- ✅ **多文件播放**：支持按顺序播放多个MCAP文件
- ✅ **循环播放**：支持单个或多个文件的循环播放
- ✅ **灵活过滤**：支持白名单、黑名单channel过滤
- ✅ **时间戳保持**：保持原始消息的时间戳


## 快速参考

### Record 命令（录制）
```bash
./mcap_recorder record [OPTIONS]

选项：
  -o <file>     输出文件名（可选，默认使用时间戳）
  -c <topics>   只录制指定的 channel（空格分隔）
  -k <topics>   不录制指定的 channel（空格分隔）
  -i <seconds>  分段录制间隔（秒）
  -h            显示帮助

示例：
  ./mcap_recorder record                              # 录制所有，自动命名
  ./mcap_recorder record -o mydata                    # 录制所有，指定文件名
  ./mcap_recorder record -c /topic1 /topic2           # 只录制指定 channel
  ./mcap_recorder record -k /debug                    # 排除调试 channel
  ./mcap_recorder record -o data -i 3600              # 每小时分段
```

### Play 命令（播放）
```bash
./mcap_recorder play <files> [OPTIONS]

选项：
  -r <factor>   播放速度（默认1.0）
  -s <seconds>  从指定秒数开始播放（默认0）
  -l            循环播放
  -c <topics>   只播放指定的 channel（空格分隔）
  -k <topics>   不播放指定的 channel（空格分隔）
  -h            显示帮助

交互：
  空格键        暂停/恢复
  Ctrl+C        停止

示例：
  ./mcap_recorder play file.mcap                      # 播放单个文件
  ./mcap_recorder play f1.mcap f2.mcap                # 播放多个文件
  ./mcap_recorder play data.mcap -r 2.0               # 2倍速播放
  ./mcap_recorder play data.mcap -s 60                # 从第60秒开始
  ./mcap_recorder play data.mcap -c /topic1 /topic2   # 只播放指定 channel
  ./mcap_recorder play data.mcap -k /debug            # 排除调试 channel
  ./mcap_recorder play data.mcap -l -r 2.0            # 循环2倍速播放
```

### Convert 命令（转换）
```bash
./mcap_recorder convert --input <file> --output <file>

示例：
  ./mcap_recorder convert --input data.record --output data.mcap
  ./mcap_recorder convert --input data.mcap --output data.record
```

## 使用方法

### 格式转换功能

#### 1. Cyber record → MCAP 转换

将Cyber record文件转换为MCAP格式：

```bash
./mcap_recorder convert --input input.record --output output.mcap
```

或者使用自动检测：

```bash
./mcap_recorder --input input.record --output output.mcap
```

#### 2. MCAP → Cyber record 转换

将MCAP文件转换为Cyber record格式：

```bash
./mcap_recorder convert --input input.mcap --output output.record
```

或者使用自动检测：

```bash
./mcap_recorder --input input.mcap --output output.record
```

**自动检测规则：**
- `.record` → `.mcap`：Cyber record转MCAP
- `.mcap` → `.record`：MCAP转Cyber record

### MCAP播包功能

#### 3. 播放单个MCAP文件

播放MCAP文件（默认播放所有channel）：

```bash
./mcap_recorder play input.mcap
```

#### 4. 播放多个MCAP文件

按顺序播放多个MCAP文件：

```bash
./mcap_recorder play file1.mcap file2.mcap file3.mcap
```

#### 5. 控制播放速度

使用 `-r` 或 `--rate` 选项控制播放速度：

```bash
# 2倍速播放
./mcap_recorder play input.mcap -r 2.0

# 0.5倍速慢放
./mcap_recorder play input.mcap -r 0.5
```

#### 6. 循环播放

使用 `-l` 或 `--loop` 选项循环播放：

```bash
./mcap_recorder play input.mcap -l
```

#### 7. 暂停和恢复播放

播放过程中按**空格键**可以暂停/恢复播放：

```bash
./mcap_recorder play input.mcap
# 播放时按空格键暂停
# 暂停时按空格键恢复
```

暂停时会输出当前统计信息，包括已播放的消息数量和字节数。

#### 8. 过滤channel播放

使用白名单只播放指定的channel（一次使用 `-c` 选项，后面跟多个 topic，空格分隔）：

```bash
# 使用 -c 或 --white-channel，后面跟多个 topic
./mcap_recorder play input.mcap -c /apollo/localization/pose /apollo/perception/obstacles

# 或使用长选项
./mcap_recorder play input.mcap --white-channel /topic1 /topic2 /topic3
```

使用黑名单排除指定的channel（一次使用 `-k` 选项，后面跟多个 topic，空格分隔）：

```bash
# 使用 -k 或 --black-channel，后面跟多个 topic
./mcap_recorder play input.mcap -k /apollo/debug /apollo/monitor

# 或使用长选项
./mcap_recorder play input.mcap --black-channel /debug1 /debug2
```

#### 9. 从指定时间开始播放

使用 `-s` 或 `--start` 选项从指定秒数开始播放：

```bash
# 从第10秒开始播放
./mcap_recorder play data.mcap -s 10

# 从第30秒开始，2倍速播放
./mcap_recorder play data.mcap -s 30 -r 2.0
```

**参数说明：**
- `-s, --start <seconds>`：从指定秒数开始播放（默认0，从头开始）

#### 10. 组合使用多个选项

```bash
# 2倍速循环播放多个文件，只播放指定channel
./mcap_recorder play file1.mcap file2.mcap -r 2.0 -l -c /topic1 /topic2

# 慢速播放，排除调试信息
./mcap_recorder play data.mcap -r 0.5 -k /debug

# 播放指定的多个channel，并排除某些子topic
./mcap_recorder play data.mcap -c /apollo/localization /apollo/perception -k /apollo/debug

# 从第60秒开始，2倍速播放，只播放指定channel
./mcap_recorder play data.mcap -s 60 -r 2.0 -c /localization /perception
```

**播放参数说明：**
- 位置参数：一个或多个 `.mcap` 文件路径
- `-r, --rate <factor>`：播放速度倍数（默认1.0，2.0表示2倍速，0.5表示半速）
- `-l, --loop`：循环播放
- `-s, --start <seconds>`：从指定秒数开始播放（默认0）
- `-c, --white-channel <topics...>`：只播放指定的channel（后面跟多个 topic，空格分隔）
- `-k, --black-channel <topics...>`：不播放指定的channel（后面跟多个 topic，空格分隔）
- `-h, --help`：显示帮助信息
- 按**空格键**：暂停/恢复播放
- 按**Ctrl+C**：停止播放

**注意：**
- 白名单和黑名单可以同时使用，黑名单优先级更高
- 设置白名单后，只会播放白名单中的channel
- 黑名单会从播放列表中排除指定的channel
- 使用 `-c` 或 `-k` 后，所有后续的非选项参数（不以 `-` 开头）都会被当作 topic
- 使用 `-s` 跳过前面的消息，从指定时间点开始播放

### 录制功能

#### 10. 录制所有channel（默认行为）

直接运行 record 命令，默认录制所有 channel，文件名自动使用时间戳：

```bash
# 录制所有channel，文件名自动生成（如：20240914_083727.mcap）
./mcap_recorder record

# 指定输出文件名
./mcap_recorder record -o output.mcap
```

#### 11. 白名单录制

只录制指定的channel（一次使用 `-c`，后面跟多个 topic，空格分隔）：

```bash
# 只录制指定的channel
./mcap_recorder record -c /apollo/localization/pose /apollo/perception/obstacles

# 指定输出文件名
./mcap_recorder record -o output.mcap -c /topic1 /topic2 /topic3
```

**参数说明：**
- `-c, --white-channel <topics...>`：指定要录制的channel（后面跟多个 topic，空格分隔）
- `-o, --output <file>`：输出文件路径（可选，默认使用时间戳）

#### 12. 黑名单录制

录制除指定channel外的所有channel（一次使用 `-k`，后面跟多个 topic，空格分隔）：

```bash
# 录制除调试信息外的所有channel
./mcap_recorder record -k /apollo/debug /apollo/monitor

# 指定输出文件名
./mcap_recorder record -o output.mcap -k /debug1 /debug2
```

**参数说明：**
- `-k, --black-channel <topics...>`：指定不录制的channel（后面跟多个 topic，空格分隔）
- `-o, --output <file>`：输出文件路径（可选，默认使用时间戳）

#### 13. 分段录制

按时间间隔自动分段录制：

```bash
# 不指定文件名，每小时分段，文件名如：20240914_083727_0.mcap, 20240914_090727_1.mcap
./mcap_recorder record -i 3600

# 指定基础文件名，每小时分段，文件名如：output_0.mcap, output_1.mcap
./mcap_recorder record -o output -i 3600
```

**参数说明：**
- `-i, --segment-interval <seconds>`：分段间隔（秒），0表示不分段
- `-o, --output <file>`：输出文件基础名称（可选）

**分段录制文件命名规则：**
- 未指定 `-o`：`20240914_083727_0.mcap`, `20240914_083727_1.mcap`, `20240914_083727_2.mcap`, ...
- 指定 `-o output`：`output_0.mcap`, `output_1.mcap`, `output_2.mcap`, ...

**注意：** 所有分段文件使用相同的基础时间戳，只有序号递增，方便识别属于同一次录制。

#### 14. 组合使用

```bash
# 录制指定channel，每小时分段
./mcap_recorder record -c /localization /perception -i 3600

# 排除调试信息，指定文件名，每30分钟分段
./mcap_recorder record -o data -k /debug -i 1800

# 白名单+黑名单+分段
./mcap_recorder record -o data -c /apollo/localization -c /apollo/perception -k /apollo/debug -i 3600
```

## 完整参数列表

### Record 命令参数

```bash
# 查看帮助信息
./mcap_recorder record --help
# 或使用简称
./mcap_recorder record -h
```

**录制参数：**
- `-o, --output <file>`：输出文件路径（可选，默认使用时间戳命名）
- `-c, --white-channel <topics...>`：只录制指定的channel（空格分隔）
- `-k, --black-channel <topics...>`：不录制指定的channel（空格分隔）
- `-i, --segment-interval <seconds>`：分段录制间隔（秒，0表示不分段）
- `-h, --help`：显示帮助信息
- `--discovery-interval <ms>`：channel发现间隔（毫秒，默认2000）

**默认行为：**
- 默认录制所有channel（除非设置了白名单）
- 默认文件名使用时间戳（如：`20240914_083727.mcap`）
- 设置分段时，所有分段使用相同的基础时间戳，序号递增（如：`20240914_083727_0.mcap`, `20240914_083727_1.mcap`）

**参数使用说明：**
- **白名单/黑名单**：`-c /topic1 /topic2 /topic3`（一次使用选项，后面跟多个 topic）
- **黑名单优先级**高于白名单
- 设置白名单后，只录制白名单中的channel
- 黑名单从录制列表中排除指定的channel
- `-c` 或 `-k` 后面的所有非选项参数（不以 `-` 开头）都会被当作 topic

### Play 命令参数

```bash
# 查看帮助信息
./mcap_recorder play --help
# 或使用简称
./mcap_recorder play -h
```

**播放参数：**
- 位置参数：一个或多个 `.mcap` 文件（必需）
- `-r, --rate <factor>`：播放速度倍数（默认1.0，2.0表示2倍速，0.5表示半速）
- `-l, --loop`：循环播放
- `-s, --start <seconds>`：从指定秒数开始播放（默认0）
- `-c, --white-channel <topics...>`：只播放指定的channel（后面跟多个 topic，空格分隔）
- `-k, --black-channel <topics...>`：不播放指定的channel（后面跟多个 topic，空格分隔）
- `-h, --help`：显示帮助信息

**播放控制：**
- **空格键**：暂停/恢复播放
- **Ctrl+C**：停止播放

**参数使用说明：**
- **白名单/黑名单**：`-c /topic1 /topic2 /topic3`（一次使用选项，后面跟多个 topic）
- **黑名单优先级**高于白名单
- 设置白名单后，只播放白名单中的channel
- 黑名单从播放列表中排除指定的channel
- `-c` 或 `-k` 后面的所有非选项参数（不以 `-` 开头）都会被当作 topic
- 使用 `-s` 可以快速跳到指定时间点开始播放，无需等待前面的内容

### Convert 命令参数

```bash
# 查看帮助信息
./mcap_recorder convert --help
# 或使用简称
./mcap_recorder convert -h
```

**转换参数：**
- `--input <file>`：输入文件路径（必需）
- `--output <file>`：输出文件路径（必需）

## 高级配置

### 自定义发现间隔

```bash
./mcap_recorder record -o output.mcap --discovery-interval 5000
```

### 组合使用白名单和黑名单

```bash
# Record 命令
./mcap_recorder record -o output.mcap -c /apollo/localization/pose -k /apollo/localization/pose/debug

# Play 命令
./mcap_recorder play data.mcap -c /apollo/localization /apollo/perception -k /apollo/debug
```

## 录制流程

1. **初始化**：创建MCAP writer，初始化统计信息
2. **启动发现**：使用Cyber Timer定期扫描新channel
3. **消息接收**：订阅匹配的channel，接收消息
4. **队列处理**：将消息放入队列，由写入线程处理
5. **MCAP写入**：将消息写入MCAP文件，包含schema和channel信息
6. **分段管理**：按配置间隔自动创建新文件
7. **清理退出**：优雅停止所有线程，关闭文件

## 性能优化

- **零拷贝设计**：使用RawMessage指针，避免消息数据拷贝
- **多线程架构**：分离消息接收和写入线程
- **智能缓存**：缓存schema和channel信息，避免重复创建
- **高效队列**：使用条件变量实现高效的消息队列

## 文件格式

录制的MCAP文件包含：
- **Schema**：Protobuf消息定义
- **Channel**：消息通道信息
- **Message**：实际消息数据和时间戳
- **Metadata**：录制统计信息和配置

## 注意事项

1. **权限要求**：需要访问Cyber RT环境的权限
2. **磁盘空间**：确保有足够的磁盘空间存储录制文件
3. **网络延迟**：在高频消息场景下，考虑网络带宽和延迟
4. **分段文件**：分段录制时，确保文件命名不会冲突
5. **黑白名单优先级**：黑名单优先级高于白名单
6. **播放暂停功能**：
   - 需要在终端中运行（非后台运行）才能使用空格键暂停
   - 暂停期间会自动调整播放时间，恢复后不会跳帧
   - 暂停时统计信息会显示 `[PAUSED]` 标记
7. **多文件播放**：播放多个文件时，每个文件会按顺序完整播放，前一个播放完成后自动播放下一个
8. **起始时间功能**：使用 `-s` 选项可以快速跳到指定时间点，适合快速定位和测试特定场景

## 故障排除

### 常见问题

1. **无法发现channel**
   - 检查Cyber RT环境是否正常运行
   - 确认channel名称拼写正确

2. **录制文件为空**
   - 检查黑白名单配置
   - 确认有消息在目标channel上发布

3. **性能问题**
   - 调整发现间隔减少CPU使用
   - 考虑使用分段录制减少单个文件大小

### 日志级别

程序使用日志系统，可以通过环境变量调整日志级别：
```bash
export GLOG_v=2  # 设置日志级别为INFO
```

## 示例场景

### 完整工作流程示例

#### 1. 数据录制 → 格式转换 → 播包回放

```bash
# 步骤1：录制数据（文件名自动生成）
./mcap_recorder record

# 或指定文件名
./mcap_recorder record -o raw_data

# 步骤2：转换为Cyber record格式（用于其他工具）
./mcap_recorder convert --input raw_data.mcap --output raw_data.record

# 步骤3：播包回放验证（可按空格暂停查看）
./mcap_recorder play raw_data.mcap
```

#### 2. 自动驾驶数据录制

```bash
# 录制定位和感知相关数据，每30分钟分段
./mcap_recorder record \
  -o driving_data \
  -c /apollo/localization/pose /apollo/perception/obstacles /apollo/sensor/camera/front_6mm /apollo/sensor/lidar32 \
  -i 1800
```

#### 3. 调试数据录制

```bash
# 录制除调试信息外的所有数据
./mcap_recorder record -o production_data -k /apollo/debug /apollo/monitor
```

#### 4. 格式转换工作流

```bash
# 将现有Cyber record转换为MCAP格式
./mcap_recorder convert --input old_data.record --output new_data.mcap

# 将MCAP转换回Cyber record格式
./mcap_recorder convert --input new_data.mcap --output restored_data.record
```

#### 5. 高级播放测试

```bash
# 以2倍速快速测试数据
./mcap_recorder play test_data.mcap -r 2.0

# 以0.5倍速慢速分析数据，播放时可按空格暂停查看
./mcap_recorder play debug_data.mcap -r 0.5

# 循环播放用于持续测试
./mcap_recorder play test_data.mcap -l -r 2.0

# 播放多个文件进行连续测试
./mcap_recorder play test1.mcap test2.mcap test3.mcap -r 2.0

# 只播放指定channel，用于特定模块测试（一次使用 -c，后面跟多个 topic）
./mcap_recorder play data.mcap -c /apollo/localization/pose /apollo/perception/obstacles

# 排除调试channel，快速播放（一次使用 -k，后面跟多个 topic）
./mcap_recorder play data.mcap -k /apollo/debug /apollo/monitor -r 2.0

# 组合使用：只播放定位和感知数据，但排除调试信息
./mcap_recorder play data.mcap -c /apollo/localization /apollo/perception -k /apollo/debug

# 从第60秒开始，2倍速测试
./mcap_recorder play data.mcap -s 60 -r 2.0
```

#### 6. 批量转换脚本

```bash
#!/bin/bash
# 批量转换所有.record文件为.mcap格式
for file in *.record; do
    base_name="${file%.*}"
    ./mcap_recorder convert --input "$file" --output "${base_name}.mcap"
    echo "Converted $file to ${base_name}.mcap"
done
```

# 免责声明
- 本软件仅对最新 ADCOS 提供支持
- 仅对 x86 平台提供支持
- 其他涉及网络配置、环境配置等报错的概不负责，请自行解决
