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

  void Render();
  bool ResetTexture(const FrameParam &param);
  void UpdateRect(SDL_Rect &texture_rect, SDL_Rect &render_rect);

 public:
  using DelayTimeCalculator = std::function<int64_t(int64_t)>;
 private:
  DelayTimeCalculator delay_time_calculator_;
 public:
  Renderer(SDL_Window *window, DelayTimeCalculator calculator);
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer& operator= (const Renderer &) = delete;

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

