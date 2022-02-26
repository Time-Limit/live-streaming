#pragma once

#include "util/util.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <string>
#include <fstream>
#include <cstdint>

namespace live {
namespace player {

class Frame;
class Sample;

class Decoder {
 public:
  Decoder();
  ~Decoder();
 public:
  bool DecodeVideoPacket(const AVStream *stream, AVCodecContext *ctx,
      const AVPacket *pkt, std::vector<Frame> *frames);
  bool DecodeAudioPacket(const AVStream *stream, AVCodecContext *ctx,
      const AVPacket *pkt, std::vector<Sample> *samples);

 private:
  using Packet2AVFrameCallback = std::function<bool(const AVFrame *)>;
  bool Packet2AVFrame(AVCodecContext *ctx, const AVPacket *pkt, const Packet2AVFrameCallback &cb);
  AVFrame *av_frame_ = nullptr;

  struct PixelData {
    static const uint8_t VIDEO_DST_SIZE = 4;
    uint8_t *data[VIDEO_DST_SIZE] = {nullptr};
    int linesize[VIDEO_DST_SIZE] = {0};
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    int height = -1;
    int width = -1;
    int data_size = -1;

    bool Reset(int w, int h, AVPixelFormat fmt);

    ~PixelData() {
      av_free(data[0]);
    }
  };
  PixelData pixel_data_;

  SwsContext *sws_context_ = nullptr;
  PixelData sws_pixel_data_;
  bool ResetSwsContext(int w, int h, AVPixelFormat fmt);
};

}
}
