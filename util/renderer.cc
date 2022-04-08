#include "util/renderer.h"
#include "util/env.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include <fstream>

namespace live {
namespace util {

// bool Renderer::ResetSwsContext(int w, int h, AVPixelFormat fmt) {
//  if (sws_pixel_data_.height == h && sws_pixel_data_.width == w &&
//  sws_pixel_data_.pix_fmt == fmt) {
//    return true;
//  }
//  if (sws_context_) {
//    sws_freeContext(sws_context_);
//    sws_context_ = nullptr;
//  }
//  sws_context_ = sws_getContext(w, h, fmt, w, h, AV_PIX_FMT_YUYV422,
//  SWS_BILINEAR, nullptr, nullptr, nullptr); if (!sws_context_) {
//    LOG_ERROR << "sws_getContext failed";
//    return false;
//  }
//  if (!sws_pixel_data_.Reset(w, h, AV_PIX_FMT_YUYV422)) {
//    return false;
//  }
//  return true;
//}
//
// bool Renderer::PixelData::Reset(int w, int h, AVPixelFormat fmt) {
//  if (w == width && h == height && pix_fmt == fmt) {
//    return true;
//  }
//  this->~PixelData();
//  PixelData();
//  data_size = av_image_alloc(data, linesize, w, h, fmt, 1);
//  if (data_size < 0) {
//    LOG_ERROR << "av_image_alloc failed";
//    return false;
//  }
//  w = width, h = height, pix_fmt = fmt;
//  return true;
//}

bool Renderer::ResetTexture(int height, int width, AVPixelFormat pix_fmt) {
  if (texture_ && param_for_texture_.height == height &&
      param_for_texture_.width == width &&
      param_for_texture_.pix_fmt == pix_fmt) {
    return true;
  }

  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }

  if (pix_fmt != AV_PIX_FMT_YUYV422) {
    LOG_ERROR << "not handled this format " << av_get_pix_fmt_name(pix_fmt);
    return false;
  }

  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YUY2,
                               SDL_TEXTUREACCESS_STREAMING, width, height);

  if (texture_ == nullptr) {
    LOG_ERROR << "create texture failed, " << SDL_GetError();
    return false;
  }

  param_for_texture_.height = height;
  param_for_texture_.width = width;
  param_for_texture_.pix_fmt = pix_fmt;

  return true;
}

void Renderer::UpdateRect(SDL_Rect& texture_rect, SDL_Rect& render_rect) {
  texture_rect.x = 0, texture_rect.y = 0;
  texture_rect.w = param_for_texture_.width;
  texture_rect.h = param_for_texture_.height;
  int w = 0, h = 0;
  SDL_GetWindowSize(window_, &w, &h);
  if (texture_rect.w * 1.0 / w > texture_rect.h * 1.0 / h) {
    render_rect.x = 0;
    render_rect.y = 0;
    render_rect.w = w;
    render_rect.h = texture_rect.h * w * 1.0 / texture_rect.w;
  } else {
    render_rect.x = 0;
    render_rect.y = 0;
    render_rect.h = h;
    render_rect.w = texture_rect.w * h * 1.0 / texture_rect.h;
  }

  render_rect.h *= 2;
  render_rect.w *= 2;

  // 居中显示
  render_rect.x = (w * 2 - render_rect.w) / 2;
  render_rect.y = (h * 2 - render_rect.h) / 2;

  // LOG_ERROR << "texture: " << texture_rect.x << ", " << texture_rect.y << ",
  // " << texture_rect.w << ", " <<texture_rect.h; LOG_ERROR << "render: " <<
  // render_rect.x << ", " << render_rect.y << ", " << render_rect.w << ", "
  // <<render_rect.h;
}

bool Renderer::ConvertToYUYV422(AVFrameWrapper& frame) {
  if (frame->format == AV_PIX_FMT_YUYV422) {
    return true;
  }

  return video_scale_helper_.Scale(frame, frame->width, frame->height,
                                   AV_PIX_FMT_YUYV422);
}

void Renderer::Render() {
  SDL_Rect texture_rect;
  SDL_Rect render_rect;

  while (is_alive_) {
    AVFrameWrapper frame;
    if (!submit_queue_.TimedGet(&frame, std::chrono::milliseconds(100))) {
      continue;
    }

    if (!ConvertToYUYV422(frame)) {
      LOG_ERROR << "ConvertToYUYV422 failed";
      break;
    }

    if (!ResetTexture(frame->height, frame->width,
                      AVPixelFormat(frame->format))) {
      LOG_ERROR << "ResetTexture failed";
      break;
    }

    int64_t delay_time = delay_time_calculator_(frame);

    //LOG_ERROR << "pts: " << frame->pts << ", delay: " << delay_time
    // << ", height: " << frame->height
    // << ", width: " << frame->width
    // << ", linesize: " << frame->linesize[0]
    // << ", pix_fmt: " << av_get_pix_fmt_name(AVPixelFormat(frame->format))
    // << ", time_base: " << frame->time_base.num << '/' <<
    // frame->time_base.den;

    if (delay_time > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(delay_time));
    }

    UpdateRect(texture_rect, render_rect);

    // static std::ofstream outfile ("test.yuv", std::ofstream::binary);
    // outfile.write((const char *)&frame.data[0], frame.data.size());
    // outfile.flush();

    SDL_UpdateTexture(texture_, nullptr, frame->data[0], frame->linesize[0]);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, &texture_rect, &render_rect);
    SDL_RenderPresent(renderer_);
  }

  is_alive_ = false;
}

Renderer::Renderer(DelayTimeCalculator calculator)
    : is_alive_(true)
    , submit_queue_(100)
    , render_future_(std::async(std::launch::async, &Renderer::Render, this))
    , delay_time_calculator_(std::move(calculator)) {
  if (!util::ThisThreadIsMainThread()) {
    throw std::string("Renderer can only be constructed on the main thread");
  }
  window_ = SDL_CreateWindow(
      nullptr, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800 /*width*/,
      800 /*height*/, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  if (window_ == nullptr) {
    throw std::string("create window failed, ") + SDL_GetError();
  }
  if (!window_) {
    throw std::string("the pointer to SDL_Window is nullptr");
  }
  renderer_ = SDL_CreateRenderer(window_, -1, 0);
  if (renderer_ == nullptr) {
    throw std::string("create render failed, ") + SDL_GetError();
  }
  if (!delay_time_calculator_) {
    throw std::string("the calculator is not callable");
  }

  SDL_AddEventWatch(&SDLEvnetFilter, reinterpret_cast<void*>(this));
}

int Renderer::SDLEvnetFilterInternal(SDL_Event* event) {
  if (event && event->type == SDL_WINDOWEVENT &&
      event->window.windowID == SDL_GetWindowID(window_)) {
    if (SDL_WINDOWEVENT_CLOSE == event->window.event) {
      LOG_ERROR << "window: " << event->window.windowID
                << " receives close event";
      Kill();
    }
  }
  // LOG_ERROR << "type: " << event->type  << ", id: " << event->window.windowID
  // << ", event: " << int(event->window.event);
  return 0;
}

Renderer::~Renderer() {
  Kill();
  SDL_DelEventWatch(&SDLEvnetFilter, reinterpret_cast<void*>(this));
}

}  // namespace util
}  // namespace live
