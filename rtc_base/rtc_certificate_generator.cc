/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rtc_certificate_generator.h"

#include <time.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/openssl_key_pair.h"
#include "rtc_base/ssl_identity.h"
namespace webrtc {

namespace {

// A certificates' subject and issuer name.
const char kIdentityName[] = "WebRTC";
const uint64_t kYearInSeconds = 365 * 24 * 60 * 60;

}  // namespace

// static
scoped_refptr<RTCCertificate> RTCCertificateGenerator::GenerateCertificate(
    const KeyParams& key_params,
    const std::optional<uint64_t>& expires_ms) {
  if (!key_params.IsValid()) {
    return nullptr;
  }

  std::unique_ptr<SSLIdentity> identity;
  if (!expires_ms) {
    identity = SSLIdentity::Create(kIdentityName, key_params);
  } else {
    uint64_t expires_s = *expires_ms / 1000;
    // Limit the expiration time to something reasonable (a year). This was
    // somewhat arbitrarily chosen. It also ensures that the value is not too
    // large for the unspecified `time_t`.
    expires_s = std::min(expires_s, kYearInSeconds);
    // TODO(torbjorng): Stop using `time_t`, its type is unspecified. It it safe
    // to assume it can hold up to a year's worth of seconds (and more), but
    // `SSLIdentity::Create` should stop relying on `time_t`.
    // See bugs.webrtc.org/5720.
    time_t cert_lifetime_s = static_cast<time_t>(expires_s);
    identity = SSLIdentity::Create(kIdentityName, key_params, cert_lifetime_s);
  }
  if (!identity) {
    return nullptr;
  }
  return RTCCertificate::Create(std::move(identity));
}

RTCCertificateGenerator::RTCCertificateGenerator(Thread* signaling_thread,
                                                 Thread* worker_thread)
    : signaling_thread_(signaling_thread), worker_thread_(worker_thread) {
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);
}

void RTCCertificateGenerator::GenerateCertificateAsync(
    const KeyParams& key_params,
    const std::optional<uint64_t>& expires_ms,
    RTCCertificateGenerator::Callback callback) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(callback);

  worker_thread_->PostTask([key_params, expires_ms,
                            signaling_thread = signaling_thread_,
                            cb = std::move(callback)]() mutable {
    scoped_refptr<RTCCertificate> certificate =
        RTCCertificateGenerator::GenerateCertificate(key_params, expires_ms);
    signaling_thread->PostTask(
        [cert = std::move(certificate), cb = std::move(cb)]() mutable {
          std::move(cb)(std::move(cert));
        });
  });
}

RTCCertificateGeneratorWithKey::RTCCertificateGeneratorWithKey(
    Thread* signaling_thread,
    Thread* worker_thread,
    absl::string_view pem_string)
    : signaling_thread_(signaling_thread), worker_thread_(worker_thread) {
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);

  key_pair_ = OpenSSLKeyPair::FromPrivateKeyPEMString(pem_string);
  RTC_DCHECK(key_pair_.get());
}

// static
scoped_refptr<RTCCertificate>
RTCCertificateGeneratorWithKey::GenerateCertificate(
    const KeyParams& key_params,
    std::unique_ptr<OpenSSLKeyPair> key_pair,
    const std::optional<uint64_t>& expires_ms) {
  if (!key_params.IsValid()) {
    return nullptr;
  }
  if (!key_pair) {
    return nullptr;
  }

  std::unique_ptr<SSLIdentity> identity;
  if (!expires_ms) {
    identity =
        SSLIdentity::Create(kIdentityName, key_params, std::move(key_pair),
                            kDefaultCertificateLifetimeInSeconds);
  } else {
    uint64_t expires_s = *expires_ms / 1000;
    // Limit the expiration time to something reasonable (a year). This was
    // somewhat arbitrarily chosen. It also ensures that the value is not too
    // large for the unspecified `time_t`.
    expires_s = std::min(expires_s, kYearInSeconds);
    // TODO(torbjorng): Stop using `time_t`, its type is unspecified. It it safe
    // to assume it can hold up to a year's worth of seconds (and more), but
    // `SSLIdentity::Create` should stop relying on `time_t`.
    // See bugs.webrtc.org/5720.
    time_t cert_lifetime_s = static_cast<time_t>(expires_s);
    identity = SSLIdentity::Create(kIdentityName, key_params,
                                   std::move(key_pair), cert_lifetime_s);
  }
  if (!identity) {
    return nullptr;
  }
  return RTCCertificate::Create(std::move(identity));
}

void RTCCertificateGeneratorWithKey::GenerateCertificateAsync(
    const KeyParams& key_params,
    const std::optional<uint64_t>& expires_ms,
    Callback callback) {
    // TODO: (FomoGoMan) moved the private member 'key_pair_', fix it 
    GenerateCertificateAsync(key_params, std::move(key_pair_), expires_ms, std::move(callback));
}

void RTCCertificateGeneratorWithKey::GenerateCertificateAsync(
    const KeyParams& key_params,
    std::unique_ptr<OpenSSLKeyPair> key_pair,
    const std::optional<uint64_t>& expires_ms,
    Callback callback) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(callback);

  worker_thread_->PostTask(
      [key_params, expires_ms, signaling_thread = signaling_thread_,
       cb = std::move(callback), key_pair = std::move(key_pair)]() mutable {
        scoped_refptr<RTCCertificate> certificate =
            RTCCertificateGeneratorWithKey::GenerateCertificate(
                key_params, std::move(key_pair), expires_ms);
        signaling_thread->PostTask(
            [cert = std::move(certificate), cb = std::move(cb)]() mutable {
              std::move(cb)(std::move(cert));
            });
      });
}

void RTCCertificateGenerator::GenerateCertificateAsync(
    const KeyParams& key_params,
    std::unique_ptr<OpenSSLKeyPair> key_pair,
    const std::optional<uint64_t>& expires_ms,
    Callback callback) {
  RTC_DCHECK(RTC_UNREACHABLE_CODE_HIT);
}

}  // namespace webrtc
