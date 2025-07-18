/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "examples/peerconnection/client/conductor.h"
#include "examples/peerconnection/client/flag_defs.h"
#include "examples/peerconnection/client/peer_connection_client.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"

class CustomSocketServer : public webrtc::PhysicalSocketServer {
 public:
  explicit CustomSocketServer()
      : conductor_(nullptr), client_(nullptr) {}
  ~CustomSocketServer() override {}

  void SetMessageQueue(webrtc::Thread* queue) override {
    message_queue_ = queue;
  }

  void set_client(PeerConnectionClient* client) { client_ = client; }
  void set_conductor(Conductor* conductor) { conductor_ = conductor; }


 protected:
  webrtc::Thread* message_queue_;
  Conductor* conductor_;
  PeerConnectionClient* client_;
};

int main(int argc, char* argv[]) {

  absl::ParseCommandLine(argc, argv);

  webrtc::Environment env =
      webrtc::CreateEnvironment(std::make_unique<webrtc::FieldTrials>(
          absl::GetFlag(FLAGS_force_fieldtrials)));

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((absl::GetFlag(FLAGS_port) < 1) || (absl::GetFlag(FLAGS_port) > 65535)) {
    printf("Error: %i is not a valid port.\n", absl::GetFlag(FLAGS_port));
    return -1;
  }

  const std::string server = absl::GetFlag(FLAGS_server);

  CustomSocketServer socket_server;
  webrtc::AutoSocketServerThread thread(&socket_server);

  webrtc::InitializeSSL();
  // Must be constructed after we set the socketserver.
  PeerConnectionClient client;
  auto conductor = webrtc::make_ref_counted<Conductor>(env, &client);
  socket_server.set_client(&client);
  socket_server.set_conductor(conductor.get());

  thread.Run();

  webrtc::CleanupSSL();
  return 0;
}
