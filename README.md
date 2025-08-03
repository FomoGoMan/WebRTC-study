**WebRTC is a free, open software project** that provides browsers and mobile
applications with Real-Time Communications (RTC) capabilities via simple APIs.
The WebRTC components have been optimized to best serve this purpose.

**Our mission:** To enable rich, high-quality RTC applications to be
developed for the browser, mobile platforms, and IoT devices, and allow them
all to communicate via a common set of protocols.

The WebRTC initiative is a project supported by Google, Mozilla and Opera,
amongst others.

### Development

See [here][native-dev] for instructions on how to get started
developing with the native code.

[Authoritative list](native-api.md) of directories that contain the
native API header files.

### More info

 * Official web site: http://www.webrtc.org
 * Master source code repo: https://webrtc.googlesource.com/src
 * Samples and reference apps: https://github.com/webrtc
 * Mailing list: http://groups.google.com/group/discuss-webrtc
 * Continuous build: https://ci.chromium.org/p/webrtc/g/ci/console
 * [Coding style guide](g3doc/style-guide.md)
 * [Code of conduct](CODE_OF_CONDUCT.md)
 * [Reporting bugs](docs/bug-reporting.md)
 * [Documentation](g3doc/sitemap.md)

[native-dev]: https://webrtc.googlesource.com/src/+/main/docs/native-code/

### Minimal Peer connection 样例
在构建目录out/Minimal创建args.gn
```
# 基础配置
target_os = "mac"  # 根据您的平台调整
target_cpu = "arm64"   # 根据您的架构调整
is_debug = false

# 测试时打开
# is_debug=true
# use_rtti=true 
# is_component_build=false

# 禁用所有音视频
rtc_enable_webrtc_audio = false
rtc_enable_video = false
rtc_include_audio_device = false
rtc_include_builtin_audio_codecs = false
rtc_include_builtin_video_codecs = false

# 启用 DataChannel 必要模块
rtc_enable_webrtc_data_channel = true
enable_dtls = true
enable_sctp = true
rtc_enable_ice = true

# 优化选项
symbol_level = 0
use_thin_lto = true

is_component_build = false  # 默认false表示静态链接
rtc_static_library = true   # 默认true表示构建静态库


# 彻底禁用所有音频相关模块
rtc_enable_webrtc_audio = false
rtc_include_audio_device = false
rtc_include_builtin_audio_codecs = false
rtc_include_opus = false
rtc_include_audio_processing = false
rtc_include_audio_mixer = false
rtc_include_audio_decoder = false
rtc_include_audio_encoder = false
rtc_include_audio_frame_processor = false
rtc_include_audio_coding = false
rtc_include_audio_mixer_impl = false
rtc_include_audio_network_adaptor = false

# 禁用视频相关模块
rtc_enable_video = false
rtc_include_builtin_video_codecs = false
rtc_use_vp8 = false
rtc_use_vp9 = false
rtc_use_h264 = false

# 强制排除媒体引擎中的音频部分
rtc_exclude_media_engine_audio = true  
```

编译样例
```bash
# release
gn gen out/Minimal
# debug
gn gen out/Default  --export-compile-commands

ninja -C out/Minimal minimal_peerconnection
```