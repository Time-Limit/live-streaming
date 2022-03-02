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
  /*
   * @Param param, 如果 param 和 current_sample_param_ 不同则重置
   * @return true 成功，false 失败
   */
  bool ResetAudioDevice(const SampleParam &param);

  /*
   * @Param userdata, 指向 Speaker 对象的指针
   * @Param stream, 缓冲区指针
   * @Param len, 缓冲区长度
   */
  static void SDLAudioDeviceCallback(void *userdata, Uint8 *stream, int len) {
    reinterpret_cast<Speaker *>(userdata)->SDLAudioDeviceCallbackInternal(stream, len);
  }

  /*
   * @Param stream, 缓冲区指针
   * @Param len, 缓冲区长度
   */
  void SDLAudioDeviceCallbackInternal(Uint8 *stream, int len);

  // 音频设备ID
  SDL_AudioDeviceID audio_device_id_ = -1;

  // audio_device_id_ 使用的参数
  SampleParam current_sample_param_;

  // 和 current_sample_param_ 的数据一致，只是格式不同
  SDL_AudioSpec desired_audio_spec_;

  // 即将提交给 SDL 的音频数据缓冲区
  std::vector<uint8_t> sample_buffer_;

  // 和 SDLAudioDeviceCallback 线程交互的队列 
  util::Queue<Sample> sample_queue_;

  // 存放外部提交数据的队列
  util::Queue<Sample> submit_queue_;
  std::future<void> speak_future_;
  bool is_stop_ = false;

  /*
   * @note 会有单独的线程执行此函数。
   * 消费 submit_queue_ 的数据，根据参数控制 audio_device_id_，并将音频数据写入 sample_queue_。
   *
   */
  void Speak();

 public:
  // 外部调用 Sumbit 后，音频并未立即播放。可通过设置类型的回调函数获悉播放时机。
  // 会在提交至SDL之前调用该函数。
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
