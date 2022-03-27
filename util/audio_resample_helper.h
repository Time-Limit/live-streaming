#pragma once

#include "util/base.h"

#include <memory>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

namespace live {
namespace util {

class AudioResampleHelper {

  struct SwrContextDeleter {
    void operator()(SwrContext *ptr) {
      swr_free(&ptr);
    }
  };

  // 用于转换格式的 SwrContext
  std::unique_ptr<SwrContext, SwrContextDeleter> swr_ctx_;

  // swr_ctx 的辅助数据
  uint64_t swr_in_channel_layout_;
  uint64_t swr_out_channel_layout_;
  int swr_in_sample_rate_;
  int swr_out_sample_rate_;
  AVSampleFormat swr_in_sample_fmt_;
  AVSampleFormat swr_out_sample_fmt_;

  bool Reset(uint64_t ic, int is, AVSampleFormat isf, uint64_t oc, int os, AVSampleFormat of);

 public:
  bool Resample(AVFrameWrapper &wrapper, int out_rate_, uint64_t out_channel_layout, AVSampleFormat out_fmt);
};

}
}
