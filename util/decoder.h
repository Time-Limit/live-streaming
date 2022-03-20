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
namespace util {

class AVFrameWrapper;

class Decoder {
 public:
  Decoder();
  ~Decoder();
 public:
  /*
   * @Param stream, pkt 所在的 AVStream
   * @Param ctx，pkt 对应的 AVCodecContext
   * @Param pkt，待解码的 AVPacket
   * @Param frames, 用于存放解码后的视频数据
   * @return true 解码成功，false 解码失败
   */
  bool DecodeVideoPacket(const AVStream *stream, AVCodecContext *ctx,
      const AVPacket *pkt, std::vector<AVFrameWrapper> *frames);
  /*
   * @Param samples, 用于存放解码后的音频数据，其他参数和 DecodeVideoPacket 相同
   * @return true 解码成功，false 解码失败
   */
  bool DecodeAudioPacket(const AVStream *stream, AVCodecContext *ctx,
      const AVPacket *pkt, std::vector<AVFrameWrapper> *samples);

 private:
  // 从 AVPacket 解码音频帧和视频帧的流程很相似，只有从 AVFrame 提取 YUV 和 PCM 的部分有差异。
  // 因此，将差异部分封装为 FrameDataExtractor。相同逻辑复用 Packet2AVFrame
  using FrameDataExtractor = std::function<bool(const AVFrame *)>;

  /*
   * @Param ctx，pkt 对应的 AVCodecContext
   * @Param pkt，待解码的 AVPacket
   * @Param extractor，AVPacket 解码后得到 AVFrame，extractor 将从这些 AVFrame 提取数据。
   * @return true 成功，false 失败
   */
  bool Packet2AVFrame(AVCodecContext *ctx, const AVPacket *pkt, const FrameDataExtractor &extractor);

  // 解码过程中复用的 AVFrame
  AVFrame *av_frame_ = nullptr;

  // 转换视频格式用的数据。
  // 因 SDL 能播放的格式优先，因此将其他 AVPixelFormat 均转换为 YUV420P，方便 SDL 播放。
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
};

}
}
