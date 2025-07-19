#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/environment/environment_factory.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"

#ifdef WEBRTC_ANDROID
#endif
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/environment/environment_factory.h"
#include "api/peer_connection_interface.h"
#include "file_transfer_handler.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/thread.h"
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
  explicit MinimalPeerConnection()  {
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

    auto env = webrtc::CreateEnvironment();
    auto audio_device = webrtc::TestAudioDeviceModule::Create(
        env,
        webrtc::TestAudioDeviceModule::CreatePulsedNoiseCapturer(32000, 48000),
        webrtc::TestAudioDeviceModule::CreateDiscardRenderer(48000), 1.0f);

    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread_.get(), worker_thread_.get(), signaling_thread_.get(),
        audio_device, webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(), nullptr, nullptr, nullptr,
        nullptr);

    if (!peer_connection_factory_) {
      std::cerr << "Failed to create PeerConnectionFactory" << std::endl;
      return;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:stun.l.google.com:19302";
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
    file_handler_  = std::make_unique<FileTransferHandler>(data_channel_);
  }

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    std::string sdp;
    desc->ToString(&sdp);
    std::ostringstream oss;
    oss << "#######################################\n"
        << "sdp:" << sdp << "end_sdp\n"
        << "#######################################\n\n"
        << std::endl;
    printColoredResult(true, oss.str());
    peer_connection_->SetLocalDescription(
        std::unique_ptr<webrtc::SessionDescriptionInterface>(desc),
        DummySetLocalDescriptionObserver::Create());
  }

  void OnFailure(webrtc::RTCError error) override {
    std::cerr << "CreateSessionDescription failed: " << error.message()
              << std::endl;
  }

  void OnStateChange() override {
    if (data_channel_ &&
        data_channel_->state() == webrtc::DataChannelInterface::kOpen) {
      std::cout << "Data channel open" << std::endl;
    }
  }

  void OnMessage(const webrtc::DataBuffer& buffer) override {
    std::string message(buffer.data.data<char>(), buffer.data.size());
    // std::cout << "Received message: " << message << std::endl;
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
    file_handler_  = std::make_unique<FileTransferHandler>(data_channel_);
  }
  rtc::scoped_refptr<webrtc::DataChannelInterface> GetDataChannel() {
    return data_channel_;
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

    // show commands prompt
    std::cout << "> " << line << std::endl;

    // parse SDP(multiple lines)
    if (line.starts_with("sdp:")) {
      reading_sdp = true;
      sdp_accumulator = line.substr(4) + "\n";
    } else if (line == "end_sdp" && reading_sdp) {
      // sdp lines ends
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
      line += "\n";  // candidates line end up with a trailing newline
      peer->AddIceCandidate(line.substr(5));
    }

    if (line.starts_with("file:")) {
      peer->SendFile(line.substr(5));
    }
  }

  return 0;
}
