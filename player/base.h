#pragma once

extern "C" {
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
}

#include <vector>


namespace live {
namespace player {

#pragma pack (1)
struct FrameParam {
  uint32_t height = 0;
  uint32_t width = 0;
  int32_t linesize = 0;
  int64_t pts = -1; // in microsecond
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

  bool IsSameWith(const FrameParam &param) const {
    return memcmp(this, &param, sizeof(FrameParam)) == 0;
  }
};
#pragma pack ()

struct Frame {
  FrameParam param;
  std::vector<uint8_t> data;
};

#pragma pack ()
struct SampleParam {
  int8_t channel_number = 0; // The number of audio channels
  int32_t sample_rate = 0; // The number of audio samples per second.
  enum AVSampleFormat sample_format = AV_SAMPLE_FMT_NONE;
  int32_t sample_number = 0;
  int64_t pts = -1; //播放时间，单位微秒  microsecond
  int64_t duration = -1; //播放时长, 单位微秒 microsecond

  bool IsSameWith(const SampleParam &rhs) const {
    return this->channel_number == rhs.channel_number 
      && this->sample_rate == rhs.sample_rate
      && this->sample_format == rhs.sample_format;
  }
};
#pragma pack ()

struct Sample {
  SampleParam param;
  std::vector<uint8_t> data;
};

}
}
