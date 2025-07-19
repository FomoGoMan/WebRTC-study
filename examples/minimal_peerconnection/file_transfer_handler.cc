// file_transfer_handler.cpp
#include "file_transfer_handler.h"
#include <fstream>
#include <iostream>

FileTransferHandler::FileTransferHandler(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : data_channel_(std::move(data_channel)) {}

void FileTransferHandler::SendFile(const std::string& file_path) {
  // 检查是否已经在传输
  if (sending_) {
    std::cerr << "Another file transfer is in progress" << std::endl;
    return;
  }
  
  // 打开文件
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << file_path << std::endl;
    return;
  }
  
  // 获取文件大小
  size_t file_size = file.tellg();
  file.seekg(0);
  
  // 获取文件名（不含路径）
  size_t pos = file_path.find_last_of("/\\");
  std::string file_name = (pos == std::string::npos) ? file_path : file_path.substr(pos + 1);
  
  sending_ = true;
  
  // 发送文件头
  SendFileHeader(file_name, file_size);
  
  // 发送文件内容
  SendFileContent(file_path);
  
  file.close();
  sending_ = false;
}

void FileTransferHandler::SendFileHeader(const std::string& file_name, size_t file_size) {
  std::vector<uint8_t> header;
  
  // Magic number (2 bytes)
  header.push_back(0xFE);
  header.push_back(0xFF);
  
  // Filename length (2 bytes, little-endian)
  uint16_t name_len = file_name.size();
  header.push_back(static_cast<uint8_t>(name_len & 0xFF));
  header.push_back(static_cast<uint8_t>((name_len >> 8) & 0xFF));
  
  // Filename
  for (char c : file_name) {
    header.push_back(static_cast<uint8_t>(c));
  }
  
  // Filesize (8 bytes, little-endian)
  for (int i = 0; i < 8; i++) {
    header.push_back(static_cast<uint8_t>((file_size >> (i * 8)) & 0xFF));
  }
  
  // 发送头
  SendChunk(header.data(), header.size());
}

void FileTransferHandler::SendFileContent(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to reopen file: " << file_path << std::endl;
    return;
  }
  
  const size_t chunk_size = 16 * 1024; // 16KB 分块
  std::vector<uint8_t> buffer(chunk_size);
  
  while (!file.eof()) {
    file.read(reinterpret_cast<char*>(buffer.data()), chunk_size);
    size_t bytes_read = file.gcount();
    
    if (bytes_read > 0) {
      SendChunk(buffer.data(), bytes_read);
    }
  }
  
  file.close();
}

void FileTransferHandler::OnMessageReceived(const webrtc::DataBuffer& buffer) {
  // 将数据传递给状态机处理
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data.data());
  state_machine_.ProcessData(data, buffer.size());
  
  // 如果传输完成，重置状态机
  if (state_machine_.state() == FileTransferState::kCompleted) {
    state_machine_.Reset();
  }
}

void FileTransferHandler::SendChunk(const uint8_t* data, size_t size) {
  if (data_channel_->state() != webrtc::DataChannelInterface::kOpen) {
    std::cerr << "Data channel not open for sending." << std::endl;
    return;
  }
  
  rtc::CopyOnWriteBuffer buffer(data, size);
  bool success = data_channel_->Send(webrtc::DataBuffer(buffer, true));
  
  if (!success) {
    std::cerr << "Failed to send data chunk." << std::endl;
  }else{
    std::cout << "Data chunk sent." << std::endl;
  }
}