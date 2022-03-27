#include "util/audio_resample_helper.h"

namespace live {
namespace util {

bool AudioResampleHelper::Reset(uint64_t ic, int is, AVSampleFormat isf, uint64_t oc, int os, AVSampleFormat of) {
  if (swr_ctx_.get()
    && swr_in_channel_layout_ == ic && swr_in_sample_rate_ == is && swr_in_sample_fmt_ == isf
    && swr_out_channel_layout_ == oc && swr_out_sample_rate_ == oc && swr_out_sample_fmt_ == of) {
    return true;
  }

  auto tmp = swr_alloc();
  if (tmp == nullptr) {
    LOG_ERROR << "swr_alloc failed";
    return false;
  }

  /* set options */
  av_opt_set_int(tmp, "in_channel_layout",    ic, 0);
  av_opt_set_int(tmp, "in_sample_rate",       is, 0);
  av_opt_set_sample_fmt(tmp, "in_sample_fmt", isf, 0);
  
  av_opt_set_int(tmp, "out_channel_layout",    oc, 0);
  av_opt_set_int(tmp, "out_sample_rate",       os, 0);
  av_opt_set_sample_fmt(tmp, "out_sample_fmt", of, 0);

  int ret = 0;
  if ((ret = swr_init(tmp)) < 0) {
    LOG_ERROR << "failed to initialize the resampling context, " << av_err2str(ret);
    swr_free(&tmp);
    return false;
  }

  swr_ctx_.reset(tmp);

  swr_in_channel_layout_ = ic;
  swr_out_channel_layout_ = oc;
  swr_in_sample_rate_ = is;
  swr_out_sample_rate_ = os;
  swr_in_sample_fmt_ = isf;
  swr_out_sample_fmt_ = of;

  return true;
}

bool AudioResampleHelper::Resample(AVFrameWrapper &wrapper, int out_rate, uint64_t out_channel_layout, AVSampleFormat out_fmt) {
  if (!Reset(wrapper->channel_layout, wrapper->sample_rate, AVSampleFormat(wrapper->format),
      out_channel_layout, out_rate, out_fmt)) {
    return false;
  }

  AVFrameWrapper new_sample(av_frame_alloc());
  if (!new_sample.GetRawPtr()) {
    LOG_ERROR << "alloc frame failed";
    return false;
  }

  new_sample->channel_layout = out_channel_layout;
  new_sample->sample_rate = out_rate;
  new_sample->format = out_fmt;
  int ret = swr_convert_frame(swr_ctx_.get(), new_sample.GetRawPtr(), wrapper.GetRawPtr());
  if (ret) {
    LOG_ERROR << "swr_convert_frame failed, err: " << av_err2str(ret);
    return false;
  }

  //TODO 还需要拷贝其他属性么？
  new_sample->pts = wrapper->pts;
  new_sample->time_base = wrapper->time_base;
  wrapper = std::move(new_sample);

  return true;
}

}
}
