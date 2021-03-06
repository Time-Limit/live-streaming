#pragma once

#include "util/audio_resample_helper.h"
#include "util/base.h"
#include "util/queue.h"
#include "util/video_scale_helper.h"

#include <future>
#include <string>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace live {
namespace util {

typedef struct OutputStream {
  AVStream* st = nullptr;
  AVCodecContext* enc = nullptr;
  AVPacket* packet = nullptr;
} OutputStream;

struct MuxerParam {
  // int64_t audio_bit_rate = 0;
  int32_t audio_sample_rate = 0;
  uint64_t audio_channel_layout = 0;
  AVSampleFormat audio_sample_format = AV_SAMPLE_FMT_NONE;
  AVRational audio_time_base = {0, 1};

  // int64_t video_bit_rate = 0;
  int64_t video_width = 0;
  int64_t video_height = 0;
  AVPixelFormat video_pix_fmt = AV_PIX_FMT_NONE;
  AVRational video_time_base = {0, 1};

  // output url
  std::string url;
};

class Muxer {
  AVFormatContext* format_context_ = nullptr;
  const AVOutputFormat* output_format_ = nullptr;
  OutputStream video_st_ = {0}, audio_st_ = {0};
  MuxerParam muxer_param_;

  VideoScaleHelper video_scale_helper_;
  AudioResampleHelper audio_resample_helper_;

  Queue<AVFrameWrapper> queue_;
  std::future<void> muxing_future_;
  bool is_alive_ = false;

 public:
  Muxer(const MuxerParam& mp);
  ~Muxer();

  bool Submit(AVFrameWrapper&& wrapper) {
    if (!is_alive_) {
      return false;
    }
    if (queue_.Size() > 16 && wrapper->width) {
      LOG_ERROR << "drop frame, type: "
                << (wrapper->channel_layout ? 'A' : 'V');
      return false;
    }
    queue_.Put(std::move(wrapper));
    return true;
  }
};

}  // namespace util
}  // namespace live
