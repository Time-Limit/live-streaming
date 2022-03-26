#include "util/speaker.h"

namespace live {
namespace util {

Speaker::~Speaker() {
  Kill();
}

void Speaker::Speak() {
  while (is_alive_) {
    AVFrameWrapper sample;
    if (!submit_queue_.TimedGet(&sample, std::chrono::milliseconds(100))) {
      continue;
    }
    int channel_number = av_get_channel_layout_nb_channels(sample->channel_layout);
    if (!ResetAudioDevice(channel_number, sample->sample_rate, AVSampleFormat(sample->format))) {
      LOG_ERROR << "ResetAudioDevice failed";
      break;
    }
    sample_queue_.Put(std::move(sample));
  }
  is_alive_ = false;
}

void Speaker::SDLAudioDeviceCallbackInternal(Uint8 *stream, int len) {
  SDL_memset(stream, 0, len);

  AVFrameWrapper next;
  while (sample_buffer_.size() < len && sample_queue_.TryToGet(&next)) {
    if (callback_) {
      callback_(next);
    }
    size_t len = next->nb_samples * av_get_bytes_per_sample(AVSampleFormat(next->format)) * av_get_channel_layout_nb_channels(AVSampleFormat(next->channel_layout));
    sample_buffer_.insert(sample_buffer_.end(), next->data[0], next->data[0] + len);
  }
  if (len > sample_buffer_.size()) {
    len = sample_buffer_.size();
  }
  if (len == 0) {
    return;
  }
  SDL_MixAudioFormat(stream, &sample_buffer_[0], desired_audio_spec_.format, len, SDL_MIX_MAXVOLUME);
  auto dst = &sample_buffer_[0];
  auto src = &sample_buffer_[0] + len;
  auto copy_size = sample_buffer_.size() - len;
  memcpy(dst, src, copy_size);
  sample_buffer_.resize(copy_size);
}

bool Speaker::ResetAudioDevice(int channel_number, int sample_rate, AVSampleFormat sample_format) {
  if (audio_device_id_ != -1 && param_for_device_.channel_number == channel_number
      && param_for_device_.sample_rate == sample_rate
      && param_for_device_.sample_format == sample_format) {
    return true;
  }

  if (audio_device_id_ != -1) {
    SDL_CloseAudioDevice(audio_device_id_);
    LOG_ERROR << "close audio device, id: " << audio_device_id_;
    audio_device_id_ = -1;
  }

  LOG_ERROR << "sample_format is " << av_get_sample_fmt_name(sample_format);

  // 转换参数
  desired_audio_spec_.freq = sample_rate;
  desired_audio_spec_.channels = channel_number;
  desired_audio_spec_.samples = 1024;

  // 转换 FFmpeg 的 sample_format 至 SDL 的
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
      desired_audio_spec_.format = AUDIO_U8; break;
    case AV_SAMPLE_FMT_S16:
      desired_audio_spec_.format = AUDIO_S16SYS; break;
    case AV_SAMPLE_FMT_S32:
      desired_audio_spec_.format = AUDIO_S32SYS; break;
    case AV_SAMPLE_FMT_FLT:
      desired_audio_spec_.format = AUDIO_F32SYS; break;
    default: {
      LOG_ERROR << "not handled this FFmpeg sample format: " << sample_format
        << ", " << av_get_sample_fmt_name(sample_format);
      return false;
    }
  }

  desired_audio_spec_.callback = &Speaker::SDLAudioDeviceCallback;
  desired_audio_spec_.userdata = this;

  LOG_INFO << "freq: " << desired_audio_spec_.freq
    << ", channels: " << int(desired_audio_spec_.channels)
    << ", samples: " << desired_audio_spec_.samples
    << ", format: " << desired_audio_spec_.format;

  auto device_id = SDL_OpenAudioDevice(nullptr, 0, &desired_audio_spec_, nullptr, 0);

  if (device_id < 2) {
    LOG_ERROR << "SDL_OpenAudioDevice failed, ret: " << device_id << ", msg: " << SDL_GetError();
    return false;
  }

  audio_device_id_ = device_id;

  param_for_device_.channel_number = channel_number;
  param_for_device_.sample_rate = sample_rate;
  param_for_device_.sample_format = sample_format;
  // 可能有些未播放的数据，清理掉吧。
  sample_buffer_.resize(0);
  sample_queue_.Clear();

  SDL_PauseAudioDevice(audio_device_id_, 0);

  LOG_INFO << "open audio device success, device id " << audio_device_id_;
  return true;
}

bool Speaker::ResetSwresampleContext(int channel_layout, int sample_rate, AVSampleFormat isf, AVSampleFormat osf) {
  if (swr_ctx_
      && swr_channel_layout_ == channel_layout && swr_sample_rate_ == sample_rate
      && swr_in_sample_fmt_ == isf && swr_out_sample_fmt_ == osf) {
    return true;
  }

  if (swr_ctx_ != nullptr) {
    swr_free(&swr_ctx_);
  }
  swr_ctx_ = swr_alloc();
  if (swr_ctx_ == nullptr) {
    LOG_ERROR << "swr_alloc failed";
    return false;
  }

  /* set options */
  av_opt_set_int(swr_ctx_, "in_channel_layout",    channel_layout, 0);
  av_opt_set_int(swr_ctx_, "in_sample_rate",       sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", isf, 0);
  
  av_opt_set_int(swr_ctx_, "out_channel_layout",    channel_layout, 0);
  av_opt_set_int(swr_ctx_, "out_sample_rate",       sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", osf, 0);
  
  /* initialize the resampling context */
  int ret = 0;
  if ((ret = swr_init(swr_ctx_)) < 0) {
    LOG_ERROR << "failed to initialize the resampling context, " << av_err2str(ret);
    swr_free(&swr_ctx_);
    return false;
  }
  swr_channel_layout_ = channel_layout;
  swr_sample_rate_  = sample_rate;
  swr_in_sample_fmt_ = isf;
  swr_out_sample_fmt_ = osf;
  return true;
}

bool Speaker::ConvertPlanarSampleToPacked(AVFrameWrapper &sample) {
  switch (sample->format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_FLT:
      return true;
    default:
      break;
  }
  AVSampleFormat in_sample_format = AVSampleFormat(sample->format);
  AVSampleFormat out_sample_format = AV_SAMPLE_FMT_NONE;
  switch (in_sample_format) {
    case AV_SAMPLE_FMT_DBL:
      out_sample_format = AV_SAMPLE_FMT_FLT;
      break;
    case AV_SAMPLE_FMT_U8P:
      out_sample_format = AV_SAMPLE_FMT_U8;
      break;
    case AV_SAMPLE_FMT_S16P:
      out_sample_format = AV_SAMPLE_FMT_S16;
      break;
    case AV_SAMPLE_FMT_S32P:
      out_sample_format = AV_SAMPLE_FMT_S32;
      break;
    case AV_SAMPLE_FMT_FLTP:
      out_sample_format = AV_SAMPLE_FMT_FLT;
      break;
    case AV_SAMPLE_FMT_DBLP:
      out_sample_format = AV_SAMPLE_FMT_FLT;
      break;
    case AV_SAMPLE_FMT_S64:
      out_sample_format = AV_SAMPLE_FMT_S32;
      break;
    case AV_SAMPLE_FMT_S64P:
      out_sample_format = AV_SAMPLE_FMT_S32;
      break;
    default:
      LOG_ERROR << "not handle this format " << av_get_sample_fmt_name(in_sample_format);
      return false;
  }

  auto channel_layout = sample->channel_layout;
  auto sample_rate = sample->sample_rate;
  if (!ResetSwresampleContext(channel_layout, sample_rate, in_sample_format, out_sample_format)) {
    LOG_ERROR << "reset swr_ctx_ failed";
    return false;
  }

  AVFrameWrapper new_sample(av_frame_alloc());
  if (!new_sample.GetRawPtr()) {
    LOG_ERROR << "alloc frame failed";
    return false;
  }

  new_sample->channel_layout = channel_layout;
  new_sample->sample_rate = sample_rate;
  new_sample->format = out_sample_format;
  new_sample->pts = sample->pts;
  new_sample->time_base = sample->time_base;

  int ret = swr_convert_frame(swr_ctx_, new_sample.GetRawPtr(), sample.GetRawPtr());
  if (ret) {
    LOG_ERROR << "swr_convert_frame failed, err: " << av_err2str(ret);
    return false;
  }

  sample = std::move(new_sample);

  return true;
}

void Speaker::Submit(AVFrameWrapper &&sample) {
  if (!ConvertPlanarSampleToPacked(sample)) {
    LOG_ERROR << "ConvertPlanarSampleToPacked failed";
    return;
  }
  if (is_alive_.load()) {
    submit_queue_.Put(std::move(sample));
  }
}

}
}
