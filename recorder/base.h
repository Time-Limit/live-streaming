#pragma once

#include "util/util.h"
#include "util/base.h"
#include "util/decoder.h"

#include <string>
#include <stdio.h>
#include <thread>
#include <future>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

namespace live {
namespace recorder {

struct InputVideoParam {
  std::string url;
  std::string input_w_x_h;
  std::string pix_fmt;
  int32_t framerate;
  int32_t output_x;
  int32_t output_y;
  int32_t output_z;
  int32_t output_w;
  int32_t output_h;
  explicit InputVideoParam(std::string param);
  InputVideoParam() = default;
};

struct InputAudioParam {
  std::string url;
  explicit InputAudioParam(const std::string &param) {
    url = param;
  }
  InputAudioParam() = default;
};

class Input {
  AVFormatContext *format_context_ = nullptr;
  const AVInputFormat *input_ = nullptr;
  int stream_idx_ = -1;
  AVStream *stream_ = nullptr;
  AVCodecContext *dec_ctx_ = nullptr;

  enum InputType {
    UNDEF = 0,
    VIDEO = 1,
    AUDIO = 2,
  };
  InputType input_type_ = InputType::UNDEF;

  bool is_alive_ = true;
  ::live::util::Decoder decoder_;
  std::future<void> decoder_future_;

  InputVideoParam input_video_param_;
  InputAudioParam input_audio_param_;

  bool InitDecodeContext(enum AVMediaType type);

  void Decode();

 public:
  using Frame = ::live::util::Frame;
  using FrameReceiver = std::function<void(Frame &&)>;
  Input(const InputVideoParam &param, FrameReceiver receiver = [](Frame &&){});

  using Sample = ::live::util::Sample;
  using SampleReceiver = std::function<void(Sample &&)>;
  Input(const InputAudioParam &param, SampleReceiver receiver = [](Sample &&){});

  ~Input();
 private:
  FrameReceiver frame_receiver_;
  SampleReceiver sample_receiver_;
};

}
}
