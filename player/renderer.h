#pragma once

#include "player/base.h"

#include "util/util.h"
#include "util/queue.h"

#include <SDL2/SDL.h>

#include <chrono>
#include <thread>
#include <future>
#include <atomic>

namespace live {
namespace player {

class Renderer {
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  FrameParam current_frame_param_;

  util::Queue<Frame> submit_queue_;

  struct WindowSize {int w; int h;};
  std::atomic<WindowSize> window_size_;

  std::future<void> render_future_;
  bool is_stop_ = false;

  /*
   * @note 依次渲染 submit_queue_ 中的数据，会有单独的线程执行该函数
   */
  void Render();
  /*
   * @Param param, 根据该参数重置 texture 的相关参数，如宽高
   * @return true 成功，false 失败
   */
  bool ResetTexture(const FrameParam &param);

  /*
   * @note 根据 current_frame_param_ 和 window_size_ 计算合适的宽高和位置，实现等比例缩放和画面居中
   */
  void UpdateRect(SDL_Rect &texture_rect, SDL_Rect &render_rect);

 public:
  // 目前音画同步的机制是视频向音频同步，因此在音频播放速率不稳定时,
  // 可用 DelayTimeCalculator 计算相应的延迟播放时间。
  // 该逻辑目前需由外部指定，未提供默认的实现方式。
  // 入参：待播放帧的 pts，单位微秒
  // 返回值：延迟时长，单位毫秒
  using DelayTimeCalculator = std::function<int64_t(int64_t)>;

 private:
  DelayTimeCalculator delay_time_calculator_;

 public:
  /*
   * @Param window，可以渲染数据的窗口
   * @Param calculator，计算延迟时间的函数对象
   */
  Renderer(SDL_Window *window, DelayTimeCalculator calculator);
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer& operator= (const Renderer &) = delete;

  /*
   * @Param frame, 将 frame 提交至待播放队列，Render 函数会依次渲染队列中的数据。
   */
  void Submit(Frame &&frame) {
    while (!is_stop_ && !submit_queue_.TimedPut(std::move(frame), std::chrono::milliseconds(100))) {
    }
  }

  bool HasPendingData() const {
    return submit_queue_.Size();
  }

  void Stop() {
    if (is_stop_ == false) {
      is_stop_ = true;
    }
  }

  bool IsStop() { return is_stop_; }

  void SetWindowSize(int w, int h) {
    window_size_.store(WindowSize{w, h});
  }
};

}
}

