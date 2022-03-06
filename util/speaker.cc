#include "util/speaker.h"

namespace live {
namespace util {

Speaker::~Speaker() {
  Kill();
  LOG_ERROR << "killed";
  speak_future_.wait();
  LOG_ERROR << "speak thread exits";
  if (audio_device_id_ != -1) {
    SDL_CloseAudioDevice(audio_device_id_);
    LOG_ERROR << "close audio device, id: " << audio_device_id_;
    audio_device_id_ = -1;
  }
}

void Speaker::Speak() {
  while (is_alive_) {
    Sample sample;
    if (!submit_queue_.TimedGet(&sample, std::chrono::milliseconds(100))) {
      continue;
    }
    if (!ResetAudioDevice(sample.param)) {
      LOG_ERROR << "ResetAudioDevice failed";
      break;
    }
    sample_queue_.Put(std::move(sample));
  }
  is_alive_ = false;
}

void Speaker::SDLAudioDeviceCallbackInternal(Uint8 *stream, int len) {
  SDL_memset(stream, 0, len);

  Sample next;
  while (sample_buffer_.size() < len && sample_queue_.TryToGet(&next)) {
    if (callback_) {
      callback_(&next);
    }
    sample_buffer_.insert(sample_buffer_.end(), next.data.begin(), next.data.end());
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

bool Speaker::ResetAudioDevice(const SampleParam &param) {
  if (current_sample_param_.IsSameWith(param)) {
    return true;
  }

  if (audio_device_id_ != -1) {
    SDL_CloseAudioDevice(audio_device_id_);
    LOG_ERROR << "close audio device, id: " << audio_device_id_;
    audio_device_id_ = -1;
  }

  // 转换参数
  desired_audio_spec_.freq = param.sample_rate;
  desired_audio_spec_.channels = param.channel_number;
  desired_audio_spec_.samples = 1;
  while (desired_audio_spec_.samples <= param.sample_number) {
    desired_audio_spec_.samples <<= 1;
  }
  desired_audio_spec_.samples >>= 1;

  // 转换 FFmpeg 的 sample_format 至 SDL 的
  switch (param.sample_format) {
    case AV_SAMPLE_FMT_U8: desired_audio_spec_.format = AUDIO_U8; break;
    case AV_SAMPLE_FMT_S16: desired_audio_spec_.format = AUDIO_S16SYS; break;
    case AV_SAMPLE_FMT_S32: desired_audio_spec_.format = AUDIO_S32SYS; break;
    case AV_SAMPLE_FMT_FLT: desired_audio_spec_.format = AUDIO_F32SYS; break;
    // case AV_SAMPLE_FMT_DBL:
    // case AV_SAMPLE_FMT_S64:
    default: {
      LOG_ERROR << "not handled this FFmpeg sample format: " << param.sample_format
        << ", " << av_get_sample_fmt_name(param.sample_format);
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

  current_sample_param_ = param;
  // 可能有些未播放的数据，清理掉吧。
  sample_buffer_.resize(0);
  sample_queue_.Clear();

  SDL_PauseAudioDevice(audio_device_id_, 0);

  LOG_INFO << "open audio device success, device id " << audio_device_id_;
  return true;
}

}
}
