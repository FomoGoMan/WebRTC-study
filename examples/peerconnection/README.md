学习 `libwebrtc` 的 `examples/peerconnection` 样例是理解 WebRTC 核心功能（如建立 P2P 连接、传输音视频/数据）的最佳实践。以下是系统的学习路径和源码导读：

---

### **1. 环境准备**
#### （1）获取代码
确保已下载完整的 `libwebrtc` 代码（通过 `gclient sync`），样例位于：
```bash
src/examples/peerconnection
```

#### （2）编译样例
在 `src` 目录下编译：
```bash
gn gen out/Default --args='is_debug=true'  # 生成调试版
ninja -C out/Default peerconnection_client  # 编译客户端
```

---

### **2. 核心文件结构**
```
peerconnection/
├── client/                  # 客户端实现（GUI/命令行）
│   ├── client_common.cc     # 公共逻辑（如信令处理）
│   ├── client_common.h
│   ├── conductor.*          # 核心控制类（管理PC、媒体流）
│   ├── flags.cc             # 命令行参数解析
│   ├── main.cc              # 程序入口
│   └── ...
├── server/                  # 信令服务器（Python）
│   └── peerconnection_server.py
├── README.md                # 使用说明
└── ...
```

---

### **3. 关键代码导读**
#### （1）**信令流程**（`conductor.cc`）
- **信令交换**：通过 `PeerConnectionClient` 类与信令服务器（Python）交互。
- **关键函数**：
  ```cpp
  void Conductor::OnSignedIn()       // 登录信令服务器
  void Conductor::OnMessageFromPeer() // 处理远端SDP/ICE候选
  ```

#### （2）**PeerConnection 生命周期**（`conductor.cc`）
- **创建 PeerConnection**：
  ```cpp
  PeerConnectionInterface::RTCConfiguration config;
  auto pc = peer_connection_factory_->CreatePeerConnection(config, ...);
  ```
- **添加媒体流**：
  ```cpp
  pc->AddTrack(video_track, {stream_id});  // 添加视频轨道
  ```

#### （3）**SDP 协商**（`conductor.cc`）
- **生成 Offer/Answer**：
  ```cpp
  pc->CreateOffer(this, ...);  // 在CreateSessionDescriptionObserver回调中处理SDP
  ```
- **设置远端 SDP**：
  ```cpp
  pc->SetRemoteDescription(std::move(desc));

#### （4）**ICE 候选收集**（`conductor.cc`）
- **回调处理**：
  ```cpp
  void OnIceCandidate(const IceCandidateInterface* candidate) {
    // 发送候选到远端
    SendMessageToPeer(SerializeCandidate(candidate));
  }
  ```

#### （5）**媒体流管理**（`conductor.cc`）
- **本地采集**：
  ```cpp
  rtc::scoped_refptr<VideoTrackInterface> video_track(
      peer_connection_factory_->CreateVideoTrack(camera_source, "camera"));
  ```
- **远端流渲染**：
  ```cpp
  void AddRemoteStream(VideoTrackInterface* track) {
    remote_renderer_->AddTrack(track);  // 绑定到UI渲染器
  }
  ```

---

### **4. 学习步骤**
#### **阶段 1：运行 Demo**
1. **启动信令服务器**：
   ```bash
   python3 src/examples/peerconnection/server/peerconnection_server.py
   ```
2. **运行客户端**：
   ```bash
   out/Default/peerconnection_client --server <SERVER_IP>
   ```
3. 观察日志，理解信令交互流程。

#### **阶段 2：代码精读**
- **重点文件**：
  - `conductor.cc`：核心逻辑（PC 创建、SDP/ICE 处理）。
  - `client_common.cc`：信令协议实现。
  - `main.cc`：UI 事件循环。
- **调试技巧**：
  - 在 `conductor.cc` 的关键回调（如 `OnIceCandidate`）打断点，观察数据流。

#### **阶段 3：修改扩展**
- **示例修改**：
  - 修改 `conductor.cc`，添加自定义数据通道：
    ```cpp
    webrtc::DataChannelInit dc_config;
    pc->CreateDataChannel("chat", &dc_config);
    ```
  - 增加日志输出，观察 ICE 候选交换过程。

#### **阶段 4：深入底层**
- **关联源码**：
  - `pc/peer_connection.{h,cc}`（WebRTC 核心实现）
  - `media/engine/webrtc_video_engine.{h,cc}`（媒体流处理）

---

### **5. 关键概念速查**
| 概念                | 对应代码位置                     | 作用                          |
|---------------------|--------------------------------|-----------------------------|
| **PeerConnection**  | `conductor.cc` 的 `pc_` 成员    | 管理连接、媒体流、数据通道          |
| **SDP Offer/Answer**| `CreateOffer()`/`SetRemoteDescription()` | 协商媒体能力           |
| **ICE Candidate**   | `OnIceCandidate()`             | 建立 NAT 穿透的路径信息           |
| **MediaStream**     | `AddTrack()`/`AddRemoteStream()` | 音视频流管理                |

---

### **6. 进阶资源**
1. **官方文档**：
   - [WebRTC Architecture](https://webrtc.org/architecture/)
   - [PeerConnection API](https://webrtc.org/getting-started/peer-connections)
2. **调试工具**：
   - Chrome://webrtc-internals（用于Web端调试）
   - Wireshark（抓包分析 ICE/STUN/TURN）

通过逐步分析 `peerconnection` 样例，你可以掌握 WebRTC 的核心流程，并能够基于此开发自定义的实时通信应用。