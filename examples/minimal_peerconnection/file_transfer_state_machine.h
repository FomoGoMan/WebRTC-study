// file_transfer_state_machine.h
#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

enum class FileTransferState {
  kInit,            // 初始状态
  kReceivingHeader, // 接收文件头状态
  kReceivingFile,   // 接收文件内容状态
  kCompleted,       // 完成状态
  kError            // 错误状态
};

class FileTransferStateMachine {
public:
  FileTransferStateMachine();
  
  void ProcessData(const uint8_t* data, size_t size);
  FileTransferState state() const { return state_; }
  
  const std::string& file_name() const { return file_name_; }
  size_t file_size() const { return file_size_; }
  size_t received_bytes() const { return received_bytes_; }
  void Reset();

private:
  void ProcessInitState(const uint8_t* data, size_t size);
  void ProcessHeaderState(const uint8_t* data, size_t size);
  void ProcessFileState(const uint8_t* data, size_t size);

  FileTransferState state_;
  
  // 文件头信息
  std::vector<uint8_t> header_buffer_;
  size_t header_bytes_received_;
  size_t expected_header_size_;
  
  // 文件信息
  std::string file_name_;
  size_t file_size_;
  size_t received_bytes_;
  std::ofstream output_file_;
};