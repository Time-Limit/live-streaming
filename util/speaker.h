#pragma once

#include <SDL2/SDL.h>

#include "util/base.h"

#include "util/util.h"
#include "util/queue.h"
#include <thread>
#include <future>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

namespace live {
namespace util {

class Speaker {
  std::atomic<bool> is_alive_;
  /*
   * @Param param, 如果 param 和 current_sample_param_ 不同则重置
   * @return true 成功，false 失败
   */
  bool ResetAudioDevice(int channel_number, int sample_rate, AVSampleFormat sample_format);

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
  struct {
    int channel_number;
    int sample_rate;
    AVSampleFormat sample_format;
  } param_for_device_;

  // 和 current_sample_param_ 的数据一致，只是格式不同
  SDL_AudioSpec desired_audio_spec_;

  // 即将提交给 SDL 的音频数据缓冲区
  std::vector<uint8_t> sample_buffer_;

  // 和 SDLAudioDeviceCallback 线程交互的队列 
  util::Queue<AVFrameWrapper> sample_queue_;

  // 存放外部提交数据的队列
  util::Queue<AVFrameWrapper> submit_queue_;
  std::future<void> speak_future_;

  // 用于转换格式的 SwrContext
  SwrContext *swr_ctx_ = nullptr;

  // swr_ctx 的辅助数据
  int swr_channel_layout_;
  int swr_sample_rate_;
  AVSampleFormat swr_in_sample_fmt_;
  AVSampleFormat swr_out_sample_fmt_;

  // 将 Planar 格式的采样转为 Packed 格式
  bool ConvertPlanarSampleToPacked(AVFrameWrapper &sample);

  bool ResetSwresampleContext(int channel_layout, int sample_rate, AVSampleFormat in, AVSampleFormat out);

  /*
   * @note 会有单独的线程执行此函数。
   * 消费 submit_queue_ 的数据，根据参数控制 audio_device_id_，并将音频数据写入 sample_queue_。
   *
   */
  void Speak();

 public:
  // 外部调用 Sumbit 后，音频并未立即播放。可通过设置类型的回调函数获悉播放时机。
  // 会在提交至SDL之前调用该函数。
  using Callback = std::function<void(const AVFrameWrapper &)>;

 private:
  Callback callback_;

 public:
  Speaker() : Speaker(nullptr) {}
  Speaker(Callback cb)
    : is_alive_(true)
    , sample_queue_(1)
    , speak_future_(std::async(std::launch::async, &Speaker::Speak, this))
    , callback_(std::move(cb)) {}
  ~Speaker();

  Speaker(const Speaker &) = delete;
  Speaker& operator=(const Speaker &) = delete;

  void Submit(AVFrameWrapper &&sample);

  bool HasPendingData() const {
    return sample_queue_.Size() || submit_queue_.Size();
  }

  void Kill() {
    if (is_alive_.exchange(false)) {
      LOG_ERROR << "speak thread is exiting";
      speak_future_.wait();
      if (audio_device_id_ != -1) {
        SDL_CloseAudioDevice(audio_device_id_);
        LOG_ERROR << "close audio device, id: " << audio_device_id_;
        audio_device_id_ = -1;
      }
      LOG_ERROR << "speak thread is exited";
    }
  }

  bool IsAlive() { return is_alive_.load(); }
};

}
}
