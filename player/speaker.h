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
  util::Queue<std::vector<uint8_t>> sample_queue_;
  util::Queue<Sample> submit_queue_;
  std::future<void> speak_future_;
  bool is_stop_ = false;

  void Speak() {
    while (!is_stop_) {
      Sample sample;
      if (!submit_queue_.TimedGet(&sample, std::chrono::milliseconds(100))) {
        continue;
      }
      if (!ResetAudioDevice(sample.param)) {
        LOG_ERROR << "ResetAudioDevice failed";
        break;
      }
      sample_queue_.Put(std::move(sample.data));
    }
    is_stop_ = true;
  }

 public:
  Speaker() : sample_queue_(1), speak_future_(std::async(std::launch::async, &Speaker::Speak, this)) {}
  ~Speaker();

  Speaker(const Speaker &) = delete;
  Speaker& operator=(const Speaker &) = delete;

  void Submit(Sample &&sample) {
    submit_queue_.Put(std::move(sample));
  }

  void Stop();
  bool IsStop() { return is_stop_; }
};

}
}
