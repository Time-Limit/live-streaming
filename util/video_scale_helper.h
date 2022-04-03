#pragma once

#include "util/base.h"
#include "util/util.h"

extern "C" {
#include <libswscale/swscale.h>
}

namespace live {
namespace util {

class VideoScaleHelper {
  // 转换视频格式用的数据。
  // 因 SDL 能播放的格式有限，因此将其他 AVPixelFormat 均转换为 YUV420P，方便
  // SDL 播放。
  struct PixelData {
    static const uint8_t VIDEO_DST_SIZE = 4;
    uint8_t* data[VIDEO_DST_SIZE] = {nullptr};
    int linesize[VIDEO_DST_SIZE] = {0};
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    int height = -1;
    int width = -1;
    int data_size = -1;

    bool Reset(int w, int h, AVPixelFormat fmt);

    ~PixelData() {
      av_free(data[0]);
    }
  };
  PixelData pixel_data_;
  int src_width_ = -1;
  int src_height_ = -1;
  AVPixelFormat src_pix_fmt_ = AV_PIX_FMT_NONE;

  struct SwsContextDeleter {
    void operator()(SwsContext* ptr) {
      sws_freeContext(ptr);
    }
  };

  std::unique_ptr<SwsContext, SwsContextDeleter> sws_context_;
  PixelData sws_pixel_data_;

  bool ResetSwsContext(int sw, int sh, AVPixelFormat sfmt, int dw, int dh,
                       AVPixelFormat dfmt);

 public:
  bool Scale(AVFrameWrapper& wrapper, int w, int h, AVPixelFormat fmt);
};

}  // namespace util
}  // namespace live
