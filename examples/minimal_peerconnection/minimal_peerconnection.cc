#include <iostream>
#include <memory>
#include <string>

#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "file_transfer_handler.h"

void printColoredResult(bool success, const std::string& message = "");

class DummySetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  static webrtc::scoped_refptr<DummySetRemoteDescriptionObserver> Create() {
    return webrtc::make_ref_counted<DummySetRemoteDescriptionObserver>();
  }

  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    if (error.ok()) {
      std::cout << "SetRemoteDescription succeeded." << std::endl;
    } else {
      std::cerr << "SetRemoteDescription failed: " << error.message()
                << std::endl;
    }
  }
};

class DummySetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  static rtc::scoped_refptr<DummySetLocalDescriptionObserver> Create() {
    return rtc::scoped_refptr<DummySetLocalDescriptionObserver>(
        new rtc::RefCountedObject<DummySetLocalDescriptionObserver>());
  }

  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    if (error.ok()) {
      std::cout << "SetLocalDescription succeeded." << std::endl;
    } else {
      std::cerr << "SetLocalDescription failed: " << error.message()
                << std::endl;
    }
  }
};

class MinimalPeerConnection : public webrtc::PeerConnectionObserver,
                              public webrtc::CreateSessionDescriptionObserver,
                              public webrtc::DataChannelObserver {
 public:
  explicit MinimalPeerConnection() {
    InitializePeerConnection();
  }

  ~MinimalPeerConnection() {
    if (peer_connection_) {
      peer_connection_->Close();
      peer_connection_ = nullptr;
    }
  }

  void InitializePeerConnection() {
    network_thread_ = rtc::Thread::CreateWithSocketServer();
    network_thread_->Start();
    worker_thread_ = rtc::Thread::Create();
    worker_thread_->Start();
    signaling_thread_ = rtc::Thread::Create();
    signaling_thread_->Start();

    // 创建无音频的 PeerConnectionFactory
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread_.get(), 
        worker_thread_.get(), 
        signaling_thread_.get(),
        nullptr,  // 无音频设备
        nullptr,  // 无音频编码器
        nullptr,  // 无音频解码器
        nullptr,  // 无视频编码器
        nullptr,  // 无视频解码器
        nullptr,  // 无音频处理
        nullptr   // 
    );

    if (!peer_connection_factory_) {
      std::cerr << "Failed to create PeerConnectionFactory" << std::endl;
      return;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer server;
    server.urls.push_back("stun:47.236.146.120:3478");
    config.servers.push_back(server);
    webrtc::PeerConnectionDependencies dependencies(this);

    auto result = peer_connection_factory_->CreatePeerConnectionOrError(
        config, std::move(dependencies));

    if (!result.ok()) {
      std::cerr << "Failed to create PeerConnection: "
                << result.error().message() << std::endl;
      return;
    }

    peer_connection_ = result.value();
  }

  void CreateOffer() {
    peer_connection_->CreateOffer(
        this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }

  void CreateAnswer() {
    peer_connection_->CreateAnswer(
        this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }

  void SetRemoteDescription(const std::string& sdp) {
    webrtc::SdpParseError error;
    webrtc::SdpType sdp_type = sdp.find("a=setup:active") != std::string::npos
                                   ? webrtc::SdpType::kAnswer
                                   : webrtc::SdpType::kOffer;
    auto desc = webrtc::CreateSessionDescription(sdp_type, sdp, &error);

    if (!desc) {
      std::cerr << "Failed to parse SDP: " << error.description << std::endl;
      return;
    }

    peer_connection_->SetRemoteDescription(
        std::move(desc), DummySetRemoteDescriptionObserver::Create());
  }

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    std::cout << "ICE gathering state changed: " << new_state << std::endl;
  }

  void AddIceCandidate(const std::string& candidate) {
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> ice_candidate(
        webrtc::CreateIceCandidate("0", 0, candidate, &error));
    if (!ice_candidate) {
      std::cerr << "Failed to parse ICE candidate: " << error.description
                << std::endl;
      return;
    }

    peer_connection_->AddIceCandidate(
        std::move(ice_candidate), [](webrtc::RTCError error) {
          if (!error.ok()) {
            std::cerr << "AddIceCandidate failed: " << error.message()
                      << std::endl;
          }
        });
  }

  // Observer 实现
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    std::cout << "Signaling state changed: " << new_state << std::endl;
  }

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    std::string candidate_str;
    candidate->ToString(&candidate_str);
    std::cout << "cand:" << candidate_str << std::endl;
  }

  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
    std::cout << "Received remote data channel" << std::endl;
    data_channel_ = channel;
    data_channel_->RegisterObserver(this);
  }

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    std::string sdp;
    desc->ToString(&sdp);
    if (desc->GetType() == webrtc::SdpType::kOffer ) {
      std::cout << "#################SDP Offer#################" << std::endl;
    }else{
      std::cout << "#################SDP Answer#################" << std::endl;
    }
    std::cout << "sdp:" << sdp<< "end_sdp" << std::endl;
    std::cout << "############################################" << std::endl;

    peer_connection_->SetLocalDescription(
        std::unique_ptr<webrtc::SessionDescriptionInterface>(desc),
        DummySetLocalDescriptionObserver::Create());

  }

  void OnFailure(webrtc::RTCError error) override {
    std::cerr << "CreateSessionDescription failed: " << error.message()
              << std::endl;
  }

  void OnStateChange() override {
    if (data_channel_ && data_channel_->state() == webrtc::DataChannelInterface::kOpen) {
        file_handler_ = std::make_unique<FileTransferHandler>(data_channel_);
        printColoredResult(true, "Data channel open");
    }

    auto pc_state = peer_connection_->peer_connection_state();
    if (pc_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
      std::cout << "Peer connection connected" << std::endl;
    }
    
    // 检查 ICE 状态
    auto ice_state = peer_connection_->ice_connection_state();
    if (ice_state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected) {
      std::cout << "ICE connected" << std::endl;
    }
    
    std::cout << "OnStateChange: " << data_channel_->DataStateString(data_channel_->state()) << std::endl;
  }

  void OnMessage(const webrtc::DataBuffer& buffer) override {
    std::string message(buffer.data.data<char>(), buffer.data.size());

    if (file_handler_) {
      file_handler_->OnMessageReceived(buffer);
    }
  }

  void SendFile(const std::string& path) {
    if (file_handler_) {
      file_handler_->SendFile(path);
    }
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeerConnection() {
    return peer_connection_;
  }

  void SetDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    data_channel_ = channel;
    data_channel_->RegisterObserver(this);
    file_handler_ = std::make_unique<FileTransferHandler>(data_channel_);
  }

 private:
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<rtc::Thread> signaling_thread_;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  std::unique_ptr<FileTransferHandler> file_handler_;
};

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " [caller|callee]" << std::endl;
    return 1;
  }

  bool is_caller = std::string(argv[1]) == "caller";
  auto peer = webrtc::make_ref_counted<MinimalPeerConnection>();

  if (is_caller) {
    webrtc::DataChannelInit config;
    // 优化数据通道配置
    // config.ordered = true; // 允许乱序传输提高吞吐量
    // config.maxRetransmits = 30; // 最大重传次数
    // config.maxRetransmitTime = 10000; // 数据包最大存活时间(ms)
    // config.protocol = "sctp";
    
    auto channel =
        peer->GetPeerConnection()->CreateDataChannelOrError("test", &config);
    if (channel.ok()) {
      peer->SetDataChannel(channel.MoveValue());
      peer->CreateOffer();
    } else {
      std::cerr << "CreateDataChannel failed: " << channel.error().message()
                << std::endl;
    }
  }

  std::string line;
  std::string sdp_accumulator;
  bool reading_sdp = false;

  while (std::getline(std::cin, line)) {
    if (line == "exit")
      break;

    // 解析 SDP（多行）
    if (line.starts_with("sdp:")) {
      reading_sdp = true;
      sdp_accumulator = line.substr(4) + "\n";
    } else if (line == "end_sdp" && reading_sdp) {
      // SDP 行结束
      peer->SetRemoteDescription(sdp_accumulator);
      reading_sdp = false;
      sdp_accumulator.clear();
      if (!is_caller) {
        peer->CreateAnswer();
      }
    } else if (reading_sdp) {
      sdp_accumulator += line + "\n";
    }

    if (line.starts_with("cand:")) {
      peer->AddIceCandidate(line.substr(5));
    }

    if (line.starts_with("file:")) {
      peer->SendFile(line.substr(5));
    }
  }

  return 0;
}
