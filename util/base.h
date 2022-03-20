#pragma once

#include "util/util.h"

extern "C" {
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
}

#include <vector>
#include <string>

namespace live {
namespace util {

class AVFrameWrapper {
  AVFrame *frame = nullptr;;
 public:
  AVFrameWrapper() { frame = nullptr; }
  AVFrameWrapper(AVFrame *&&f) {
    frame = f;
    f = nullptr;
  }
  AVFrameWrapper(const AVFrame *f) {
    frame = av_frame_alloc();
    if (!frame) {
      throw std::string("no memory to alloc AVFrame");
    }
    av_frame_ref(frame, f);
  }
  
  AVFrameWrapper(const AVFrameWrapper &rhs) {
    if (!rhs.frame) {
      return;
    }
    frame = av_frame_alloc();
    if (!frame) {
      throw std::string("no memory to alloc AVFrame");
    }
    av_frame_ref(frame, rhs.frame);
  }
  AVFrameWrapper(AVFrameWrapper &&rhs) {
    frame = rhs.frame;
    rhs.frame = nullptr;
  }

  AVFrameWrapper& operator= (const AVFrameWrapper &rhs) {
    if (!frame) {
      frame = av_frame_alloc();
      if (!frame) {
        throw std::string("no memory to alloc AVFrame");
      }
    }
    av_frame_ref(frame, rhs.frame);
    return *this;
  }
  AVFrameWrapper& operator= (AVFrameWrapper &&rhs) {
    av_frame_free(&frame);
    frame = rhs.frame;
    rhs.frame = nullptr;
    return *this;
  }

  ~AVFrameWrapper() {
    av_frame_free(&frame);
  }

  AVFrame* operator->() { return frame; }
  const AVFrame* operator->() const { return frame; }
  AVFrame* GetRawPtr() { return frame; }
  const AVFrame* GetRawPtr() const { return frame; }
};

}
}
