use std::{collections::{BTreeMap, HashMap, HashSet, VecDeque}, io::Read, sync::Arc, convert::TryInto};

use foxglove_data_loader::{
    DataLoader, DataLoaderArgs, Initialization, Message, MessageIterator, MessageIteratorArgs,
    BackfillArgs, Problem,
    reader::{self},
};

use prost::Message as ProstMessage;
use prost_types::{FileDescriptorSet, FileDescriptorProto};
use anyhow::Context;
use foxglove::Schema;
use std::io::Cursor;
use lz4_flex::block::{decompress_size_prepended, decompress};

#[cfg(not(target_arch = "wasm32"))]
use bzip2::read::BzDecoder;
#[cfg(target_arch = "wasm32")]
use bzip2_rs::DecoderReader;

mod record;
use record::*;

mod proto_desc;
use proto_desc::ProtoDesc as CyberProtoDesc;

#[cfg(test)]
mod tests {
    use super::*;
    use foxglove_data_loader::DataLoaderArgs;
    
    #[test]
    fn test_record_data_loader_creation() {
        let args = DataLoaderArgs {
            paths: vec!["test.record".to_string()],
        };
        
        let loader = RecordDataLoader::new(args);
        assert_eq!(loader.paths.len(), 1);
        assert_eq!(loader.paths[0], "test.record");
        assert!(loader.channels.is_empty());
        assert!(loader.header.is_none());
    }
    
    #[test]
    fn test_message_iterator_empty() {
        let iterator = RecordMessageIterator::empty();
        assert!(iterator.messages.is_empty());
        assert_eq!(iterator.current_index, 0);
        assert!(iterator.channel_ids.is_empty());
    }
    
    #[test]
    fn test_section_type_enum() {
        assert_eq!(SectionType::SectionHeader as i32, 0);
        assert_eq!(SectionType::SectionChunkHeader as i32, 1);
        assert_eq!(SectionType::SectionChunkBody as i32, 2);
        assert_eq!(SectionType::SectionIndex as i32, 3);
        assert_eq!(SectionType::SectionChannel as i32, 4);
    }
    
    #[test]
    fn test_compress_type_enum() {
        assert_eq!(CompressType::CompressNone as i32, 0);
        assert_eq!(CompressType::CompressBz2 as i32, 1);
        assert_eq!(CompressType::CompressLz4 as i32, 2);
        assert_eq!(CompressType::CompressZstd as i32, 3);
    }
}

// 包含测试工具模块
#[cfg(test)]
mod test_utils;

#[derive(Default)]
struct RecordDataLoader {
    paths: Vec<String>,
    /// Content for each file: (file_index, content)
    file_contents: Vec<Arc<Vec<u8>>>,
    /// Index of timestamp to list of (file_index, position, channel_name) for messages
    /// Using Vec to handle multiple messages with the same timestamp
    /// file_index: u32 (file index, sufficient for reasonable number of files)
    /// position: u64 (byte position in file, can be very large for big files)
    message_indexes: BTreeMap<u64, Vec<(u32, u64, String)>>,
    /// Per-channel message count
    channel_message_counts: BTreeMap<String, u64>,
    /// Channel information (merged from all files) - uses record::Channel
    channels: Vec<record::Channel>,
    /// Header information from first file
    header: Option<Header>,
    /// Header information for each file (for compression type lookup)
    file_headers: Vec<Header>,
    /// Channel ID mapping
    channel_ids: BTreeMap<String, u16>,
    /// Schema data per channel (FileDescriptorSet serialized as bytes)
    channel_schemas: BTreeMap<String, Vec<u8>>,
    /// Channels that were successfully registered with schemas
    registered_channels: HashSet<String>,
    /// Time ranges from all files: (begin_time, end_time) per file
    file_time_ranges: Vec<(u64, u64)>,
}

impl DataLoader for RecordDataLoader {
    type MessageIterator = RecordMessageIterator;
    type Error = anyhow::Error;

    fn new(args: DataLoaderArgs) -> Self {
        let DataLoaderArgs { paths } = args;
        
        // Validate that at least one file is provided
        if paths.is_empty() {
            panic!("DataLoader requires at least one file path");
        }
        
        Self {
            paths,
            ..Default::default()
        }
    }

    fn initialize(&mut self) -> Result<Initialization, Self::Error> {
        // Collect paths first to avoid borrowing conflicts
        let paths = self.paths.clone();
        
        // Clear time ranges from previous initialization
        self.file_time_ranges.clear();
        
        // Process all files
        for (file_index_usize, path) in paths.iter().enumerate() {
            // Convert to u32 (file_index) - reasonable number of files won't exceed u32::MAX
            let file_index = file_index_usize as u32;
            
            let mut reader = reader::open(path);
            let size = reader.size();
            
            let mut buf = vec![0u8; size as usize];
            reader
                .read_exact(&mut buf)
                .with_context(|| format!("failed reading record data from {}", path))?;

            let content = Arc::new(buf);
            self.file_contents.push(content.clone());
            
            // Parse each file (this will collect time range from header)
            self.parse_record_file(file_index, &content)?;
        }

        let mut init = Initialization::builder();
        
        // Set start and end time from all files (min start, max end)
        // Use time ranges collected from headers during parsing
        if !self.file_time_ranges.is_empty() {
            let min_start = self.file_time_ranges.iter().map(|(start, _)| *start).min().unwrap();
            let max_end = self.file_time_ranges.iter().map(|(_, end)| *end).max().unwrap();
            init = init.start_time(min_start).end_time(max_end);
        } else if let Some(header) = &self.header {
            // Fallback to first file's header if no times collected
            if let (Some(start_time), Some(end_time)) = (header.begin_time, header.end_time) {
                init = init.start_time(start_time).end_time(end_time);
            }
        }

        // Add channels and create channel IDs with protobuf schemas
        for channel in &self.channels {
            if let Some(name) = &channel.name {
                let count = *self.channel_message_counts.get(name).unwrap_or(&0);
                
                // Convert Cyber ProtoDesc to FileDescriptorSet and add as schema
                if let Some(proto_desc_bytes) = &channel.proto_desc {
                    match Self::cyber_proto_desc_to_fd_set(proto_desc_bytes) {
                        Ok(fd_set) => {
                            // Serialize FileDescriptorSet to bytes
                            let mut schema_bytes = Vec::new();
                            if fd_set.encode(&mut schema_bytes).is_ok() {
                                // Get schema name from message_type, or use channel name as fallback
                                let schema_name = channel.message_type
                                    .as_deref()
                                    .unwrap_or(name);
                                // Create Schema with "protobuf" encoding
                                // Schema data contains FileDescriptorSet (protobuf format)
                                // Note: init.add_schema() expects foxglove::Schema, not foxglove_data_loader::Schema
                                let schema = Schema {
                                    name: schema_name.to_string(),
                                    encoding: "protobuf".to_string(),
                                    data: schema_bytes.into(),
                                };
                                
                                // Add schema and get LinkedSchema
                                // Important: Must set message_encoding on LinkedSchema before adding channels
                                // Reference: https://github.com/foxglove/foxglove-sdk/blob/main/rust/foxglove_data_loader/src/tests.rs
                                let linked_schema = init.add_schema(schema)
                                    .message_encoding("protobuf");
                                
                                // Add channel linked to this schema
                                // This will use the message_encoding set above
                                let channel_builder = linked_schema
                                    .add_channel(name)
                                    .message_count(count);
                                
                                let great_channel_id = channel_builder.id();
                                self.channel_ids.insert(name.clone(), great_channel_id);
                                self.registered_channels.insert(name.clone());
                                
                                // Store schema bytes for reference
                                let mut stored_bytes = Vec::new();
                                if fd_set.encode(&mut stored_bytes).is_ok() {
                                    self.channel_schemas.insert(name.clone(), stored_bytes);
                                }
                            }
                        }
                        Err(e) => {
                            // Add problem to initialization instead of printing
                            init = init.add_problem(
                                Problem::error(format!("Failed to convert proto_desc for channel {}: {}", name, e))
                                    .tip("The channel's protobuf descriptor could not be converted. This channel will be skipped.")
                            );
                            // Skip channel without valid schema to avoid payloadKeys errors
                            continue;
                        }
                    }
                } else {
                    // No proto_desc available; skip to avoid unsupported protobuf without schema
                    init = init.add_problem(
                        Problem::warn(format!("No proto_desc for channel {}, skipping registration", name))
                            .tip("The channel does not have a protobuf descriptor. Ensure the record file contains complete channel metadata.")
                    );
                    continue;
                }
            }
        }

        Ok(init.build())
    }

    fn create_iter(
        &mut self,
        args: MessageIteratorArgs,
    ) -> Result<Self::MessageIterator, Self::Error> {
        let start_time = args.start_time.unwrap_or(0);
        let end_time = args.end_time.unwrap_or(u64::MAX);

        // Find the range of messages within the time window
        // Flatten the Vec entries to handle multiple messages with the same timestamp
        let mut message_range: Vec<(u64, u32, u64, String)> = Vec::new();
        for (&ts, entries) in self.message_indexes.range(start_time..=end_time) {
            for (file_idx, pos, channel_name) in entries {
                // Only include messages for channels that were registered with schemas
                if self.registered_channels.contains(channel_name) {
                    message_range.push((ts, *file_idx, *pos, channel_name.clone()));
                }
            }
        }

        Ok(RecordMessageIterator {
            file_contents: self.file_contents.clone(),
            channel_ids: self.channel_ids.clone(),
            messages: message_range,
            current_index: 0,
            cached_chunk: None,
            file_headers: self.file_headers.clone(),
        })
    }
    /// Get the latest messages on specified channels at or before the given time.
    /// 
    /// This is used by Foxglove Studio when the user seeks to a specific time point.
    /// It returns the most recent message for each requested channel up to (and including)
    /// the specified time, which allows the UI to display the current state of all channels
    /// at that moment.
    /// 
    /// # Arguments
    /// * `args` - Contains the time point and list of channel IDs to backfill
    /// 
    /// # Returns
    /// A vector of messages, one per requested channel (or fewer if no messages exist
    /// for some channels before the specified time)
    fn get_backfill(&mut self, args: BackfillArgs) -> Result<Vec<Message>, Self::Error> {
        let backfill_time = args.time;
        let requested_channel_ids: HashSet<u16> = args.channels.iter().copied().collect();
        
        // Build a map from channel_id to channel_name for reverse lookup
        let channel_id_to_name: HashMap<u16, String> = self.channel_ids
            .iter()
            .filter(|(_, channel_id)| requested_channel_ids.contains(channel_id))
            .map(|(name, channel_id)| (*channel_id, name.clone()))
            .collect();
        
        // For each requested channel, find the latest message at or before backfill_time
        let mut backfill_messages = Vec::new();
        
        for (&channel_id, channel_name) in &channel_id_to_name {
            // Skip if channel is not registered
            if !self.registered_channels.contains(channel_name) {
                continue;
            }
            
            // Find the latest message for this channel at or before backfill_time
            // We iterate backwards through message_indexes to find the most recent message
            let mut latest_message: Option<(u64, u32, u64)> = None; // (timestamp, file_index, position)
            
            // Search through message_indexes in reverse order (most recent first)
            // We need to find messages where timestamp <= backfill_time
            for (&timestamp, entries) in self.message_indexes.range(..=backfill_time).rev() {
                // Check if any entry in this timestamp is for our channel
                for (file_idx, pos, entry_channel_name) in entries {
                    if entry_channel_name == channel_name {
                        latest_message = Some((timestamp, *file_idx, *pos));
                        break;
                    }
                }
                // If we found a message, stop searching (we're iterating backwards, so this is the latest)
                if latest_message.is_some() {
                    break;
                }
            }
            
            // If we found a message, read it and add to result
            if let Some((timestamp, file_index, position)) = latest_message {
                // Create a temporary iterator to reuse the read logic
                let mut temp_iterator = RecordMessageIterator {
                    file_contents: self.file_contents.clone(),
                    channel_ids: self.channel_ids.clone(),
                    messages: vec![(timestamp, file_index, position, channel_name.clone())],
                    current_index: 0,
                    cached_chunk: None,
                    file_headers: self.file_headers.clone(),
                };
                
                // Read the message
                match temp_iterator.read_message_at_position(file_index, position, channel_name, timestamp) {
                    Ok(protobuf_bytes) => {
                        backfill_messages.push(Message {
                            channel_id,
                            log_time: timestamp,
                            publish_time: timestamp,
                            data: protobuf_bytes,
                        });
                    },
                    Err(e) => {
                        // Runtime error: cannot use Problem API here as this occurs after initialization.
                        // Log error but continue with other channels to provide partial backfill data.
                        // Note: This error should ideally be rare if initialization was successful.
                        eprintln!("Failed to read backfill message for channel {} at time {}: {}", 
                            channel_name, timestamp, e);
                    }
                }
            }
        }
        
        Ok(backfill_messages)
    }
}


impl RecordDataLoader {
    /// Convert i32 to CompressType
    fn i32_to_compress_type(value: i32) -> CompressType {
        match value {
            0 => CompressType::CompressNone,
            1 => CompressType::CompressBz2,
            2 => CompressType::CompressLz4,
            3 => CompressType::CompressZstd,
            _ => CompressType::CompressNone, // Default to None for unknown values
        }
    }
    
    /// Decompress data based on compression type
    /// 
    /// Note: Compression is applied to the entire chunk body, not individual messages.
    /// The compressed data is the serialized ChunkBody protobuf message.
    fn decompress_data(compressed_data: &[u8], compress_type: CompressType, raw_size: Option<u64>) -> Result<Vec<u8>, anyhow::Error> {
        match compress_type {
            CompressType::CompressNone => {
                // No compression, return as-is
                Ok(compressed_data.to_vec())
            },
            CompressType::CompressBz2 => {
                #[cfg(not(target_arch = "wasm32"))]
                {
                    // Use bzip2 crate for Bz2 decompression (non-WASM)
                    let mut decoder = BzDecoder::new(Cursor::new(compressed_data));
                    let mut decompressed = Vec::new();
                    decoder.read_to_end(&mut decompressed)
                        .context("Failed to decompress Bz2 data")?;
                    Ok(decompressed)
                }
                #[cfg(target_arch = "wasm32")]
                {
                    // Use bzip2-rs for Bz2 decompression (WASM)
                    let mut decoder = DecoderReader::new(Cursor::new(compressed_data));
                    let mut decompressed = Vec::new();
                    decoder.read_to_end(&mut decompressed)
                        .context("Failed to decompress Bz2 data")?;
                    Ok(decompressed)
                }
            },
            CompressType::CompressLz4 => {
                // Use lz4_flex for LZ4 decompression
                // Cyber record format: LZ4 block format, entire chunk body is compressed
                // Try size_prepended first (if data has size prefix), otherwise use raw_size if available
                if let Some(uncompressed_size) = raw_size {
                    // Use raw_size from ChunkHeader if available
                    decompress(compressed_data, uncompressed_size as usize)
                        .map_err(|e| anyhow::anyhow!("Failed to decompress LZ4 data with raw_size {}: {}", uncompressed_size, e))
                } else {
                    // Fallback: try size_prepended format (data may have size prefix)
                    decompress_size_prepended(compressed_data)
                        .map_err(|e| anyhow::anyhow!("Failed to decompress LZ4 data (tried size_prepended): {}", e))
                }
            },
            CompressType::CompressZstd => {
                // Use zstd for Zstd decompression
                // Zstd format includes size information, so decode_all should work
                zstd::decode_all(compressed_data)
                    .context("Failed to decompress Zstd data")
            },
        }
    }
    
    /// Convert Cyber ProtoDesc (tree format) to Google FileDescriptorSet (flat format)
    /// 
    /// This function converts the Cyber record format's tree-structured ProtoDesc
    /// into the flat FileDescriptorSet format used by MCAP/Foxglove.
    fn cyber_proto_desc_to_fd_set(cyber_proto_desc_bytes: &[u8]) -> Result<FileDescriptorSet, anyhow::Error> {
        // Parse Cyber ProtoDesc
        let cyber_proto_desc = CyberProtoDesc::decode(cyber_proto_desc_bytes)
            .context("Failed to decode Cyber ProtoDesc")?;
        
        let mut fd_set = FileDescriptorSet::default();
        let mut seen = HashSet::new();
        let mut queue = VecDeque::new();
        
        queue.push_back(cyber_proto_desc);
        
        while let Some(current) = queue.pop_front() {
            // Parse FileDescriptorProto from desc bytes
            if let Some(desc_bytes) = current.desc {
                let file_desc_proto = FileDescriptorProto::decode(desc_bytes.as_slice())
                    .context("Failed to decode FileDescriptorProto")?;
                
                // Get file name for deduplication
                let file_name = file_desc_proto.name.as_deref().unwrap_or("");
                
                // Add to FileDescriptorSet if not seen
                if seen.insert(file_name.to_string()) {
                    fd_set.file.push(file_desc_proto);
                }
            }
            
            // Add dependencies to queue
            for dep in current.dependencies {
                queue.push_back(dep);
            }
        }
        
        Ok(fd_set)
    }
    
    fn parse_record_file(&mut self, file_index: u32, content: &Arc<Vec<u8>>) -> Result<(), anyhow::Error> {
        // Step 1: Read header from position 0
        let header = {
            self.read_header(content, 0)?
        };
        
        // Store time range from this file's header
        // The header contains begin_time and end_time which are the actual timestamps
        // from the record file, more accurate than calculating from messages
        if let (Some(begin_time), Some(end_time)) = (header.begin_time, header.end_time) {
            self.file_time_ranges.push((begin_time, end_time));
        }
        
        // Store header for this file (for compression type lookup)
        // Ensure file_headers has enough capacity
        while self.file_headers.len() <= file_index as usize {
            self.file_headers.push(Header::default());
        }
        self.file_headers[file_index as usize] = header.clone();
        
        // Only store header from first file (for backward compatibility)
        if file_index == 0 {
            self.header = Some(header.clone());
        }
        
        // Step 2: Read index from header.index_position
        if let Some(index_position) = header.index_position {
            if index_position > 0 && (index_position as usize) < content.len() {
                // Read index first (immutable borrow)
                let index = {
                    self.read_index(content, index_position as usize)?
                };
                
                // Collect chunk body positions and channel info (no borrow of self.content)
                let mut chunk_body_positions = Vec::new();
                let mut channels_to_add = Vec::new();
                
                // Step 3: Process index entries to extract channels
                for single_index in &index.indexes {
                    if let Some(section_type) = single_index.r#type {
                        match section_type {
                            4 => { // SECTION_CHANNEL
                                // Extract channel info from channel_cache
                                if let Some(single_index::Cache::ChannelCache(channel_cache)) = &single_index.cache {
                                    if let Some(name) = &channel_cache.name {
                                        // Create Channel from cache
                                        let channel = Channel {
                                            name: Some(name.clone()),
                                            message_type: channel_cache.message_type.clone(),
                                            proto_desc: channel_cache.proto_desc.clone(),
                                        };
                                        channels_to_add.push((name.clone(), channel));
                                    }
                                }
                            },
                            2 => { // SECTION_CHUNK_BODY
                                // Collect position for later processing
                                // Keep as u64 to avoid overflow for large files
                                // Note: Compression is applied to the entire chunk body (serialized ChunkBody protobuf)
                                if let Some(position) = single_index.position {
                                    chunk_body_positions.push(position);
                                }
                            },
                            _ => {
                                // Other index types (chunk headers, etc.) - skip for now
                            }
                        }
                    }
                }
                
                // Now modify self (mutable borrow)
                for (name, channel) in channels_to_add {
                    self.channels.push(channel);
                    self.channel_message_counts.insert(name, 0);
                }
                
                // Step 4: Process chunk bodies to index messages
                for position in chunk_body_positions {
                    // Use a small scope to limit the borrow of content
                    // Convert u64 position to usize only when needed for array indexing
                    let position_usize = position as usize;
                    if position_usize > content.len() {
                        continue; // Skip invalid positions
                    }
                    let updates = {
                        self.index_chunk_body(content, position_usize, &header)?
                    };
                    // Apply updates outside the borrow
                    for (time, channel_name) in updates {
                        // Store with file_index for multi-file support
                        // Use Vec to handle multiple messages with the same timestamp
                        self.message_indexes
                            .entry(time)
                            .or_insert_with(Vec::new)
                            .push((file_index, position, channel_name.clone()));
                        let counter = self.channel_message_counts
                            .entry(channel_name)
                            .or_insert(0);
                        *counter += 1;
                    }
                }
            }
        }

        Ok(())
    }
    
    fn read_header(&self, content: &[u8], position: usize) -> Result<Header, anyhow::Error> {
        let mut pos = position;
        
        // Read section: 4 bytes type + 4 bytes padding + 8 bytes size
        if pos + 16 > content.len() {
            anyhow::bail!("Not enough bytes for section header");
        }
        
        let section_type = u32::from_le_bytes(content[pos..pos+4].try_into()?);
        pos += 4;
        pos += 4; // Skip padding
        let section_size = u64::from_le_bytes(content[pos..pos+8].try_into()?);
        pos += 8;
        
        if section_type != 0 {
            anyhow::bail!("Expected SECTION_HEADER (0), got {}", section_type);
        }
        
        if pos + section_size as usize > content.len() {
            anyhow::bail!("Section size exceeds file size");
        }
        
        let section_data = &content[pos..pos + section_size as usize];
        Header::decode(section_data)
            .context("Failed to decode Header")
    }
    
    fn read_index(&self, content: &[u8], position: usize) -> Result<Index, anyhow::Error> {
        let mut pos = position;
        
        // Read section: 4 bytes type + 4 bytes padding + 8 bytes size
        if pos + 16 > content.len() {
            anyhow::bail!("Not enough bytes for section header");
        }
        
        let section_type = u32::from_le_bytes(content[pos..pos+4].try_into()?);
        pos += 4;
        pos += 4; // Skip padding
        let section_size = u64::from_le_bytes(content[pos..pos+8].try_into()?);
        pos += 8;
        
        if section_type != 3 {
            anyhow::bail!("Expected SECTION_INDEX (3), got {}", section_type);
        }
        
        if pos + section_size as usize > content.len() {
            anyhow::bail!("Section size exceeds file size");
        }
        
        let section_data = &content[pos..pos + section_size as usize];
        Index::decode(section_data)
            .context("Failed to decode Index")
    }
    
    fn index_chunk_body(&self, content: &[u8], position: usize, header: &Header) -> Result<Vec<(u64, String)>, anyhow::Error> {
        // Note: position parameter is usize for array indexing, but we receive u64 from record file
        // Caller should validate that position fits in usize before calling
        let mut pos = position;
        let mut updates = Vec::new();
        
        // Read section: 4 bytes type + 4 bytes padding + 8 bytes size
        if pos + 16 > content.len() {
            return Ok(updates); // Not enough bytes, skip
        }
        
        let section_type = u32::from_le_bytes(content[pos..pos+4].try_into()?);
        pos += 4;
        pos += 4; // Skip padding
        let section_size = u64::from_le_bytes(content[pos..pos+8].try_into()?);
        pos += 8;
        
        if section_type != 2 {
            return Ok(updates); // Not a chunk body, skip
        }
        
        // Validate section size
        let section_size_usize = section_size as usize;
        if pos + section_size_usize > content.len() {
            return Ok(updates); // Section size exceeds file size, skip
        }
        
        // Ensure we have enough data
        if section_size_usize == 0 {
            return Ok(updates); // Empty section, skip
        }
        
        let compressed_data = &content[pos..pos + section_size_usize];
        
        // Get compression type from header
        let compress_type = header.compress
            .map(Self::i32_to_compress_type)
            .unwrap_or(CompressType::CompressNone);
        
        // Decompress data if needed
        // Note: Compression is applied to the entire chunk body (serialized ChunkBody protobuf)
        // raw_size is not available from index, so pass None (will try size_prepended for LZ4)
        let decompressed_data = Self::decompress_data(compressed_data, compress_type, None)
            .context("Failed to decompress chunk body data")?;
        
        if let Ok(chunk_body) = ChunkBody::decode(decompressed_data.as_slice()) {
            // Process messages in chunk body
            for message in chunk_body.messages {
                if let (Some(channel_name), Some(time), Some(msg_content)) =
                    (message.channel_name, message.time, message.content)
                {
                    // Validate message content is not empty
                    if !msg_content.is_empty() {
                        updates.push((time, channel_name));
                    }
                }
            }
        }
        
        Ok(updates)
    }
}

struct RecordMessageIterator {
    file_contents: Vec<Arc<Vec<u8>>>,
    channel_ids: BTreeMap<String, u16>,
    messages: Vec<(u64, u32, u64, String)>, // (timestamp, file_index, position, channel_name)
    current_index: usize,
    cached_chunk: Option<(u32, u64, ChunkBody)>, // (file_index, position, chunk_body)
    file_headers: Vec<Header>, // Header for each file (for compression type lookup)
}

impl RecordMessageIterator {
    #[allow(unused)]
    fn empty() -> Self {
        Self {
            file_contents: Vec::new(),
            channel_ids: Default::default(),
            messages: Vec::new(),
            current_index: 0,
            cached_chunk: None,
            file_headers: Vec::new(),
        }
    }
    
    fn read_message_at_position(&mut self, file_index: u32, position: u64, channel_name: &str, timestamp: u64) -> Result<Vec<u8>, anyhow::Error> {
        // Get content for this file
        let content = self.file_contents.get(file_index as usize)
            .ok_or_else(|| anyhow::anyhow!("File index {} out of range", file_index))?;
        
        // Convert position to usize for array indexing
        // Validate that position fits in usize
        let position_usize = position as usize;
        if position_usize > content.len() {
            anyhow::bail!("Position {} exceeds file size {}", position, content.len());
        }
        
        // Check if we have the chunk cached
        let chunk_body = if let Some((cached_file_idx, cached_pos, ref cached_chunk)) = self.cached_chunk {
            if cached_file_idx == file_index && cached_pos == position {
                cached_chunk
            } else {
                // Need to load new chunk
                // Use position_usize for array indexing
                let mut pos = position_usize;
                
                if pos + 16 > content.len() {
                    anyhow::bail!("Not enough bytes for section header");
                }
                
                let section_type = u32::from_le_bytes(content[pos..pos+4].try_into()?);
                pos += 4;
                pos += 4; // Skip padding
                let section_size = u64::from_le_bytes(content[pos..pos+8].try_into()?);
                pos += 8;
                
                if section_type != 2 {
                    anyhow::bail!("Expected SECTION_CHUNK_BODY (2), got {}", section_type);
                }
                
                let section_size_usize = section_size as usize;
                if pos + section_size_usize > content.len() {
                    anyhow::bail!("Section size {} exceeds file size {} at position {}", 
                        section_size_usize, content.len(), pos);
                }
                
                let compressed_data = &content[pos..pos + section_size_usize];
                
                // Get header for this file to determine compression type
                let header = self.file_headers.get(file_index as usize)
                    .ok_or_else(|| anyhow::anyhow!("Header not found for file index {}", file_index))?;
                
                // Get compression type from header
                let compress_type = header.compress
                    .map(RecordDataLoader::i32_to_compress_type)
                    .unwrap_or(CompressType::CompressNone);
                
                // Decompress data if needed
                // Note: Compression is applied to the entire chunk body (serialized ChunkBody protobuf)
                // raw_size is not available here, so pass None (will try size_prepended for LZ4)
                let decompressed_data = RecordDataLoader::decompress_data(compressed_data, compress_type, None)
                    .context(format!("Failed to decompress chunk body at position {}", pos))?;
                
                let chunk = ChunkBody::decode(decompressed_data.as_slice())
                    .context(format!("Failed to decode ChunkBody at position {}, size {}", pos, section_size_usize))?;
                self.cached_chunk = Some((file_index, position, chunk));
                &self.cached_chunk.as_ref().unwrap().2
            }
        } else {
            // No cache, load chunk
            // Use position_usize for array indexing
            let mut pos = position_usize;
            
            if pos + 16 > content.len() {
                anyhow::bail!("Not enough bytes for section header");
            }
            
            let section_type = u32::from_le_bytes(content[pos..pos+4].try_into()?);
            pos += 4;
            pos += 4; // Skip padding
            let section_size = u64::from_le_bytes(content[pos..pos+8].try_into()?);
            pos += 8;
            
            if section_type != 2 {
                anyhow::bail!("Expected SECTION_CHUNK_BODY (2), got {}", section_type);
            }
            
            let section_size_usize = section_size as usize;
            if pos + section_size_usize > content.len() {
                anyhow::bail!("Section size {} exceeds file size {} at position {}", 
                    section_size_usize, content.len(), pos);
            }
            
            if section_size_usize == 0 {
                anyhow::bail!("Section size is zero at position {}", pos);
            }
            
            let compressed_data = &content[pos..pos + section_size_usize];
            
            // Get header for this file to determine compression type
            let header = self.file_headers.get(file_index as usize)
                .ok_or_else(|| anyhow::anyhow!("Header not found for file index {}", file_index))?;
            
            // Get compression type from header
            let compress_type = header.compress
                .map(RecordDataLoader::i32_to_compress_type)
                .unwrap_or(CompressType::CompressNone);
            
            // Decompress data if needed
            // Note: Compression is applied to the entire chunk body (serialized ChunkBody protobuf)
            // raw_size is not available here, so pass None (will try size_prepended for LZ4)
            let decompressed_data = RecordDataLoader::decompress_data(compressed_data, compress_type, None)
                .context(format!("Failed to decompress chunk body at position {}", pos))?;
            
            let chunk = ChunkBody::decode(decompressed_data.as_slice())
                .context(format!("Failed to decode ChunkBody at position {}, size {}", pos, section_size_usize))?;
            self.cached_chunk = Some((file_index, position, chunk));
            &self.cached_chunk.as_ref().unwrap().2
        };
        
        // Find the message with matching channel_name and timestamp
        for message in &chunk_body.messages {
            if let (Some(msg_channel_name), Some(msg_time), Some(content)) =
                (&message.channel_name, message.time, &message.content)
            {
                if msg_channel_name == channel_name && msg_time == timestamp {
                    // Validate message content is not empty
                    if content.is_empty() {
                        anyhow::bail!("Message content is empty for channel {} at timestamp {}", channel_name, timestamp);
                    }
                    
                    // Validate message content size (protobuf messages should have reasonable size)
                    if content.len() > 100_000_000 {
                        anyhow::bail!("Message content size {} is suspiciously large for channel {} at timestamp {}", 
                            content.len(), channel_name, timestamp);
                    }
                    
                    return Ok(content.clone());
                }
            }
        }
        
        anyhow::bail!("Message not found at position {} for channel {} with timestamp {}", position, channel_name, timestamp);
    }
}

impl MessageIterator for RecordMessageIterator {
    type Error = anyhow::Error;

    fn next(&mut self) -> Option<Result<Message, Self::Error>> {
        if self.current_index >= self.messages.len() {
            return None;
        }

        let (timestamp, file_index, position, channel_name) = self.messages[self.current_index].clone();
        self.current_index += 1;

        // Get channel ID for this message
        let Some(channel_id) = self.channel_ids.get(&channel_name).copied() else {
            // Runtime error: channel was not registered during initialization (likely missing/invalid schema).
            // This should be rare - channels without valid schemas should have been reported as problems during initialization.
            // Skip this message and continue to avoid payloadKeys errors in Foxglove Studio.
            // Note: Cannot use Problem API here as this occurs during message iteration, not initialization.
            eprintln!("Skipping message for unregistered channel {} (this should have been reported during initialization)", channel_name);
            return self.next();
        };
        
        // Read the actual protobuf message content
        // For protobuf encoding, we pass the raw binary data directly
        // Foxglove will decode it using the provided schema
        match self.read_message_at_position(file_index, position, &channel_name, timestamp) {
            Ok(protobuf_bytes) => {
                // Pass protobuf binary data directly
                // Foxglove Studio will decode it using the schema provided during channel registration
                Some(Ok(Message {
                    channel_id,
                    log_time: timestamp,
                    publish_time: timestamp,
                    data: protobuf_bytes,
                }))
            },
            Err(e) => Some(Err(e)),
        }
    }
}

foxglove_data_loader::export!(RecordDataLoader);
