// file_transfer_handler.h
#pragma once
#include <string>
#include "api/data_channel_interface.h"
#include "file_transfer_state_machine.h"

class FileTransferHandler  {
public:
  explicit FileTransferHandler(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  
  // This function blocks current caller thread until transfer complete
  void SendFile(const std::string& file_path);
  void OnMessageReceived(const webrtc::DataBuffer& buffer);
  
  // 状态查询
  bool is_transferring() const { return sending_; }
  const std::string& file_name() const { return state_machine_.file_name(); }
  size_t file_size() const { return state_machine_.file_size(); }
  size_t received_bytes() const { return state_machine_.received_bytes(); }
  bool is_completed() const { 
    return state_machine_.state() == FileTransferState::kCompleted; 
  }

private:
  void SendChunk(const uint8_t* data, size_t size);
  void SendFileHeader(const std::string& file_name, size_t file_size);
  void SendFileContent(const std::string& file_path);
  
  webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  FileTransferStateMachine state_machine_;
  bool sending_ = false;
};
