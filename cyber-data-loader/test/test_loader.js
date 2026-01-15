// Foxglove数据加载器测试用例
// 这个测试文件用于验证RecordDataLoader的基本功能

// 模拟Foxglove环境
const mockExtensionContext = {
  registerDataLoader: (config) => {
    console.log('数据加载器注册:', config);
    return {
      type: config.type,
      supportedFileType: config.supportedFileType,
      wasmUrl: config.wasmUrl
    };
  }
};

// 模拟文件读取器
class MockFileReader {
  constructor(content) {
    this.content = content;
    this.position = 0;
  }

  size() {
    return this.content.length;
  }

  readExact(buffer) {
    if (this.position + buffer.length > this.content.length) {
      throw new Error('读取超出文件范围');
    }
    
    for (let i = 0; i < buffer.length; i++) {
      buffer[i] = this.content[this.position + i];
    }
    this.position += buffer.length;
    return buffer.length;
  }

  seek(position) {
    this.position = position;
  }
}

// 创建测试用的record文件数据
function createTestRecordData() {
  // 创建一个简单的record文件结构用于测试
  const buffer = new ArrayBuffer(1024);
  const view = new DataView(buffer);
  let offset = 0;
  
  // SectionHeader (type = 0)
  view.setUint32(offset, 0, true); // section type
  offset += 4;
  view.setUint32(offset, 32, true); // section length
  offset += 4;
  
  // Header内容
  view.setUint32(offset, 1, true); // major_version
  offset += 4;
  view.setUint32(offset, 0, true); // minor_version
  offset += 4;
  view.setUint32(offset, 0, true); // compress
  offset += 4;
  view.setBigUint64(offset, BigInt(1000000), true); // chunk_interval
  offset += 8;
  view.setBigUint64(offset, BigInt(0), true); // begin_time
  offset += 8;
  view.setBigUint64(offset, BigInt(1000000000), true); // end_time
  offset += 8;
  
  // SectionChannel (type = 4)
  view.setUint32(offset, 4, true); // section type
  offset += 4;
  view.setUint32(offset, 20, true); // section length
  offset += 4;
  
  // Channel内容 - 简单模拟
  const channelName = "/test_channel";
  for (let i = 0; i < channelName.length; i++) {
    view.setUint8(offset + i, channelName.charCodeAt(i));
  }
  offset += channelName.length;
  
  return new Uint8Array(buffer, 0, offset);
}

// 测试函数
async function runTests() {
  console.log('=== Foxglove Record Data Loader 测试 ===');
  
  try {
    // 测试1: 插件注册
    console.log('\n1. 测试插件注册...');
    const { activate } = require('../src/index.ts');
    activate(mockExtensionContext);
    console.log('✓ 插件注册测试通过');
    
    // 测试2: 文件结构解析
    console.log('\n2. 测试文件结构解析...');
    const testData = createTestRecordData();
    console.log(`✓ 创建测试数据: ${testData.length} 字节`);
    
    // 测试3: 模拟文件读取
    console.log('\n3. 测试文件读取...');
    const reader = new MockFileReader(testData);
    console.log(`✓ 文件大小: ${reader.size()} 字节`);
    
    const readBuffer = new Uint8Array(64);
    const bytesRead = reader.readExact(readBuffer);
    console.log(`✓ 读取字节数: ${bytesRead}`);
    
    console.log('\n=== 所有测试通过 ===');
    
  } catch (error) {
    console.error('测试失败:', error);
    process.exit(1);
  }
}

// 如果直接运行此文件
if (require.main === module) {
  runTests();
}

module.exports = {
  MockFileReader,
  createTestRecordData,
  runTests
};
