#include "util/video_scale_helper.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
}

namespace live {
namespace util {

bool VideoScaleHelper::PixelData::Reset(int w, int h, AVPixelFormat fmt) {
  if (w == width && h == height && pix_fmt == fmt) {
    return true;
  }
  this->~PixelData();
  PixelData();
  data_size = av_image_alloc(data, linesize, w, h, fmt, 1);
  if (data_size < 0) {
    LOG_ERROR << "av_image_alloc failed, error: " << av_err2str(data_size);
    return false;
  }
  this->width = w;
  this->height = h;
  this->pix_fmt = fmt;
  return true;
}

bool VideoScaleHelper::ResetSwsContext(int sw, int sh, AVPixelFormat sfmt, int dw, int dh, AVPixelFormat dfmt) {
  if (sws_context_.get()
    && pixel_data_.data_size >= 0
    && src_width_ == sw && src_height_ == sh && src_pix_fmt_ == sfmt
    && pixel_data_.width == dw && pixel_data_.height == dh && pixel_data_.pix_fmt == dfmt) {
    return true;
  }
  auto tmp = sws_getContext(sw, sh, sfmt, dw, dh, dfmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (tmp == nullptr) {
    LOG_ERROR << "sws_getContext failed";
    return false;
  }
  if (!sws_pixel_data_.Reset(dw, dh, dfmt)) {
    return false;
  }
  sws_context_.reset(tmp);
  src_width_ = sw;
  src_height_ = sh;
  src_pix_fmt_ = sfmt;
  return true;
}

bool VideoScaleHelper::Scale(AVFrameWrapper &wrapper, int w, int h, AVPixelFormat fmt) {
  if (!ResetSwsContext(wrapper->width, wrapper->height, AVPixelFormat(wrapper->format), w, h, fmt)) {
    return false;
  }

  AVFrameWrapper new_frame(av_frame_alloc());
  if (!new_frame.GetRawPtr()) {
    LOG_ERROR << "alloc frame failed";
    return false;
  }

  int ret = sws_scale_frame(sws_context_.get(), new_frame.GetRawPtr(), wrapper.GetRawPtr());
  if (ret < 0) {
    LOG_ERROR << "scale failed, error: " << av_err2str(ret);
    return false;
  }

  //TODO 还需要拷贝其他值么？
  new_frame->pts = wrapper->pts;
  new_frame->time_base = wrapper->time_base;
  wrapper = std::move(new_frame);

  return true;
}

}
}
