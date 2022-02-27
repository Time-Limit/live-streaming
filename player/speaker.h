#pragma once

#include <SDL2/SDL.h>

#include "player/base.h"

#include "util/util.h"
#include "util/queue.h"
#include <thread>
#include <future>

namespace live {
namespace player {

class Speaker {
  SDL_AudioDeviceID audio_device_id_ = -1;
  SampleParam current_sample_param_;
  SDL_AudioSpec desired_audio_spec_;
  SDL_AudioSpec obtained_audio_spec_;

  bool ResetAudioDevice(const SampleParam &sample);
  static void SDLAudioDeviceCallback(void *userdata, Uint8 *stream, int len) {
    reinterpret_cast<Speaker *>(userdata)->SDLAudioDeviceCallbackInternal(stream, len);
  }
  void SDLAudioDeviceCallbackInternal(Uint8 *stream, int len);
  std::vector<uint8_t> sample_buffer_;
  util::Queue<Sample> sample_queue_;
  util::Queue<Sample> submit_queue_;
  std::future<void> speak_future_;
  bool is_stop_ = false;

  void Speak();

 public:
  using Callback = std::function<void(const Sample *)>;
 private:
  Callback callback_;
 public:
  Speaker() : Speaker(nullptr) {}
  Speaker(Callback cb)
    : sample_queue_(1)
    , speak_future_(std::async(std::launch::async, &Speaker::Speak, this))
    , callback_(std::move(cb)) {}
  ~Speaker();

  Speaker(const Speaker &) = delete;
  Speaker& operator=(const Speaker &) = delete;

  void Submit(Sample &&sample) {
    submit_queue_.Put(std::move(sample));
  }

  bool HasPendingData() const {
    return sample_queue_.Size() || submit_queue_.Size();
  }

  void Stop() { is_stop_ = true; }
  bool IsStop() { return is_stop_; }
};

}
}
