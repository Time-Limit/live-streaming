#pragma once

#include "util/base.h"

#include "util/util.h"
#include "util/queue.h"

#include <SDL2/SDL.h>

#include <chrono>
#include <thread>
#include <future>
#include <atomic>

extern "C" {
#include <libswscale/swscale.h>
}

namespace live {
namespace util {

class Renderer {
  std::atomic<bool> is_alive_;

  SDL_Window *window_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;

  static int SDLEvnetFilter(void *userdata, SDL_Event *event) {
    return reinterpret_cast<Renderer *>(userdata)->SDLEvnetFilterInternal(event);
  }
  int SDLEvnetFilterInternal(SDL_Event *event);

  struct {
    int height;
    int width;
    int pix_fmt;
  } param_for_texture_;

  util::Queue<AVFrameWrapper> submit_queue_;

  std::future<void> render_future_;

  /*
   * @note 依次渲染 submit_queue_ 中的数据，会有单独的线程执行该函数
   */
  void Render();
  /*
   * @Param param, 根据该参数重置 texture 的相关参数，如宽高
   * @return true 成功，false 失败
   */
  bool ResetTexture(int height, int width, AVPixelFormat pix_fmt);

  /*
   * @note 根据 current_frame_param_ 和 window size 计算合适的宽高和位置，实现等比例缩放和画面居中
   */
  void UpdateRect(SDL_Rect &texture_rect, SDL_Rect &render_rect);

 public:
  // 目前音画同步的机制是视频向音频同步，因此在音频播放速率不稳定时,
  // 可用 DelayTimeCalculator 计算相应的延迟播放时间。
  // 该逻辑目前需由外部指定，未提供默认的实现方式。
  // 入参：待播放帧的 pts，单位微秒
  // 返回值：延迟时长，单位毫秒
  using DelayTimeCalculator = std::function<int64_t(const AVFrameWrapper &)>;

 private:
  DelayTimeCalculator delay_time_calculator_;

 private:
  // 转换视频格式用的数据。
  // 因 SDL 能播放的格式有限，因此将其他 AVPixelFormat 均转换为 YUV420P，方便 SDL 播放。
  struct PixelData {
    static const uint8_t VIDEO_DST_SIZE = 4;
    uint8_t *data[VIDEO_DST_SIZE] = {nullptr};
    int linesize[VIDEO_DST_SIZE] = {0};
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    int height = -1;
    int width = -1;
    int data_size = -1;

    bool Reset(int w, int h, AVPixelFormat fmt);

    ~PixelData() {
      av_free(data[0]);
    }
  };
  PixelData pixel_data_;

  SwsContext *sws_context_ = nullptr;
  PixelData sws_pixel_data_;
  bool ResetSwsContext(int w, int h, AVPixelFormat fmt);
  bool ConvertToYUYV422(AVFrameWrapper &frame);

 public:
  /*
   * @Param window，可以渲染数据的窗口
   * @Param calculator，计算延迟时间的函数对象
   */
  Renderer(DelayTimeCalculator calculator);
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer& operator= (const Renderer &) = delete;

  /*
   * @Param frame, 将 frame 提交至待播放队列，Render 函数会依次渲染队列中的数据。
   */
  void Submit(AVFrameWrapper &&frame) {
    while (is_alive_ && !submit_queue_.TimedPut(std::move(frame), std::chrono::milliseconds(100))) {
    }
  }

  bool HasPendingData() const {
    return submit_queue_.Size();
  }

  void Kill() {
    if (is_alive_.exchange(false)) {
      LOG_ERROR << "rendering thread is exiting";

      render_future_.wait();

      if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
      }

      if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
      }

      SDL_DestroyTexture(texture_);
      texture_ = nullptr;

      LOG_ERROR << "rendering thread is exited";
    }
  }

  bool IsAlive() { return is_alive_; }
};

}
}

