#pragma once

#include "util/base.h"
#include "util/decoder.h"
#include "util/util.h"

#include <stdio.h>
#include <future>
#include <string>
#include <thread>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

namespace live {
namespace recorder {

struct InputVideoParam {
  std::string url;
  std::string input_w_x_h;
  std::string pix_fmt;
  int32_t framerate;
  explicit InputVideoParam(std::string param);
  InputVideoParam() = default;
};

struct InputAudioParam {
  std::string url;
  explicit InputAudioParam(const std::string& param) {
    url = param;
  }
  InputAudioParam() = default;
};

class Input {
 public:
  struct AgreedUponParam {
    int32_t width = 0;
    int32_t height = 0;
    AVPixelFormat pixel_format = AV_PIX_FMT_NONE;

    uint64_t channel_layout;
    int32_t sample_rate;
    AVSampleFormat sample_format = AV_SAMPLE_FMT_NONE;

    AVRational time_base;
  };

 private:
  AgreedUponParam agreed_upon_param_;

  AVFormatContext* format_context_ = nullptr;
  const AVInputFormat* input_ = nullptr;
  int stream_idx_ = -1;
  AVStream* stream_ = nullptr;
  AVCodecContext* dec_ctx_ = nullptr;

  enum InputType {
    UNDEF = 0,
    VIDEO = 1,
    AUDIO = 2,
  };
  InputType input_type_ = InputType::UNDEF;

  bool is_alive_ = true;
  ::live::util::Decoder decoder_;
  std::future<void> decoder_future_;
  //
  InputVideoParam input_video_param_;
  InputAudioParam input_audio_param_;

  bool InitDecodeContext(enum AVMediaType type);

  void Decode();

 public:
  using AVFrameWrapper = util::AVFrameWrapper;
  using FrameReceiver = std::function<void(AVFrameWrapper&&)>;

  Input(
      const InputVideoParam& param,
      FrameReceiver receiver = [](AVFrameWrapper&&) {});
  Input(
      const InputAudioParam& param,
      FrameReceiver receiver = [](AVFrameWrapper&&) {});

  bool Run();
  void Kill();
  bool IsAlive() const {
    return is_alive_;
  }
  const AVCodecContext* GetCodecContext() const {
    return dec_ctx_;
  }
  const AVStream* GetStream() const {
    return stream_;
  }

  const AgreedUponParam& GetAgreedUponParam() const {
    return agreed_upon_param_;
  }

  ~Input();

 private:
  FrameReceiver frame_receiver_;
};

}  // namespace recorder
}  // namespace live
