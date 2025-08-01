// file_transfer_state_machine.cpp
#include "file_transfer_state_machine.h"
#include <iostream>
#include <cstring>

FileTransferStateMachine::FileTransferStateMachine() {
  Reset();
}

void FileTransferStateMachine::Reset() {
  state_ = FileTransferState::kInit;
  header_buffer_.clear();
  header_bytes_received_ = 0;
  expected_header_size_ = 0;
  file_name_.clear();
  file_size_ = 0;
  received_bytes_ = 0;
  
  if (output_file_.is_open()) {
    output_file_.close();
  }
}

void FileTransferStateMachine::ProcessData(const uint8_t* data, size_t size) {
  if (size == 0) return;

  switch (state_) {
    case FileTransferState::kInit:
      ProcessInitState(data, size);
      break;
    case FileTransferState::kReceivingHeader:
      ProcessHeaderState(data, size);
      break;
    case FileTransferState::kReceivingFile:
      ProcessFileState(data, size);
      break;
    case FileTransferState::kCompleted:
    case FileTransferState::kError:
      // 忽略数据，状态未改变
      break;
  }
}

void FileTransferStateMachine::ProcessInitState(const uint8_t* data, size_t size) {
  // 验证Magic Number (0xFE 0xFF)
  if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
    state_ = FileTransferState::kReceivingHeader;
    
    // 重置头缓冲区，为解析头做准备
    header_buffer_.clear();
    header_buffer_.insert(header_buffer_.end(), data, data + size);
    header_bytes_received_ = size;
    
    // 最小头大小：magic(2) + filename_len(2) + filesize(8) = 12字节
    if (header_bytes_received_ > 12) {
      ProcessHeaderState(nullptr, 0);
    }
  } else {
    // Magic number不匹配，进入错误状态
    state_ = FileTransferState::kError;
    std::cerr << "Invalid magic number in file header" << std::endl;
  }
}

void FileTransferStateMachine::ProcessHeaderState(const uint8_t* data, size_t size) {
  // 添加到头缓冲区
  if (data && size > 0) {
    header_buffer_.insert(header_buffer_.end(), data, data + size);
    header_bytes_received_ += size;
  }

  // 检查是否已收到足够解析filename_len的数据
  if (header_bytes_received_ >= 4) {
    // 解析filename_len (2字节，小端)
    uint16_t filename_len = (header_buffer_[3] << 8) | header_buffer_[2];
    
    // 计算预期的完整头大小
    expected_header_size_ = 4 + filename_len + 8; // magic(2) + filename_len(2) + filename(n) + filesize(8)

    // 检查是否已收到完整头
    if (header_bytes_received_ >= expected_header_size_) {
      // 解析文件名
      file_name_ = std::string(
        reinterpret_cast<const char*>(&header_buffer_[4]), 
        filename_len
      );
      // parse filename 'xxx.zip' to 'xxx_download.zip'
      size_t pos = file_name_.find_last_of('.');
      if (pos != std::string::npos) {
        file_name_ = file_name_.substr(0, pos) + "_download" + file_name_.substr(pos);
      }

      
      // 解析文件大小 (8字节，小端)
      file_size_ = 0;
      for (int i = 0; i < 8; ++i) {
        file_size_ |= static_cast<uint64_t>(header_buffer_[4 + filename_len + i]) << (i * 8);
      }
      
      // 准备写入文件
      output_file_.open(file_name_, std::ios::binary | std::ios::trunc);
      if (!output_file_.is_open()) {
        state_ = FileTransferState::kError;
        std::cerr << "Failed to open file for writing: " << file_name_ << std::endl;
        return;
      }
      
      // 切换到接收文件状态
      state_ = FileTransferState::kReceivingFile;
      received_bytes_ = 0;
      
      // 处理头后的剩余数据（如果有）
      size_t file_data_offset = expected_header_size_;
      if (header_bytes_received_ > expected_header_size_) {
        size_t remaining_data_size = header_bytes_received_ - expected_header_size_;
        ProcessFileState(&header_buffer_[file_data_offset], remaining_data_size);
      }
    }
  }
}

void FileTransferStateMachine::ProcessFileState(const uint8_t* data, size_t size) {
  if (!output_file_.is_open()) {
    state_ = FileTransferState::kError;
    std::cerr << "File not open for writing" << std::endl;
    return;
  }

  // 计算当前要写入的数据量
  size_t bytes_to_write = size;
  if (received_bytes_ + size > file_size_) {
    bytes_to_write = file_size_ - received_bytes_;
  }

  // 写入文件
  output_file_.write(reinterpret_cast<const char*>(data), bytes_to_write);
  received_bytes_ += bytes_to_write;

  // 检查是否完成
  if (received_bytes_ >= file_size_) {
    output_file_.close();
    state_ = FileTransferState::kCompleted;
    std::cout << "File transfer completed: " << file_name_ 
              << " (" << file_size_ << " bytes)" << std::endl;
  }
}