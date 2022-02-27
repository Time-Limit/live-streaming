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
  uint32_t height = 0; // 高度
  uint32_t width = 0;  // 宽度
  int32_t linesize = 0; // 行宽
  int64_t pts = -1; // 播放时间，单位微秒，microsecond
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE; // 格式，当前只会是 yuv420p

  // @Param param 判断和 *this 是否相同
  // @return 所有字段相同则返回 True，反之返回 False
  bool IsSameWith(const FrameParam &param) const {
    return memcmp(this, &param, sizeof(FrameParam)) == 0;
  }
};
#pragma pack ()

struct Frame {
  FrameParam param; // 该帧的参数
  std::vector<uint8_t> data; // 该帧的像素数据
};

#pragma pack ()
struct SampleParam {
  int8_t channel_number = 0; // The number of audio channels
  int32_t sample_rate = 0; // The number of audio samples per second.
  enum AVSampleFormat sample_format = AV_SAMPLE_FMT_NONE;
  int32_t sample_number = 0; // 采样数量
  int64_t pts = -1; //播放时间，单位微秒  microsecond
  int64_t duration = -1; //播放时长, 单位微秒 microsecond

  // @Param param 判断和 *this 是否相同
  // @return 除sample_number, pts 和 duration 以外的所有字段相同则返回 True，反之返回 False
  bool IsSameWith(const SampleParam &rhs) const {
    return this->channel_number == rhs.channel_number 
      && this->sample_rate == rhs.sample_rate
      && this->sample_format == rhs.sample_format;
  }
};
#pragma pack ()

struct Sample {
  SampleParam param; // 采样参数
  std::vector<uint8_t> data; // 采样点数据
};

}
}
