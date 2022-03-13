#include "util/renderer.h"
#include "util/env.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
}

#include <fstream>

namespace live {
namespace util {

bool Renderer::ResetSwsContext(int w, int h, AVPixelFormat fmt) {
  if (sws_pixel_data_.height == h && sws_pixel_data_.width == w && sws_pixel_data_.pix_fmt == fmt) {
    return true;
  }
  if (sws_context_) {
    sws_freeContext(sws_context_);
    sws_context_ = nullptr;
  }
  sws_context_ = sws_getContext(w, h, fmt, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!sws_context_) {
    LOG_ERROR << "sws_getContext failed";
    return false;
  }
  if (!sws_pixel_data_.Reset(w, h, AV_PIX_FMT_YUV420P)) {
    return false;
  }
  return true;
}

bool Renderer::PixelData::Reset(int w, int h, AVPixelFormat fmt) {
  if (w == width && h == height && pix_fmt == fmt) {
    return true;
  }
  this->~PixelData();
  PixelData();
  data_size = av_image_alloc(data, linesize, w, h, fmt, 1);
  if (data_size < 0) {
    LOG_ERROR << "av_image_alloc failed";
    return false;
  }
  w = width, h = height, pix_fmt = fmt;
  return true;
}

bool Renderer::ResetTexture(const FrameParam &param) {
  if (param.IsSameWith(current_frame_param_)) {
    return true;
  }
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }

  if (param.pix_fmt != AV_PIX_FMT_YUV420P) {
    LOG_ERROR << "not handled this format " << av_get_pix_fmt_name(param.pix_fmt);
    return false;
  }

  texture_ = SDL_CreateTexture(renderer_,
      SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
      param.width, param.height);

  if (texture_ == nullptr) {
    LOG_ERROR << "create texture failed, " << SDL_GetError();
    return false;
  }

  current_frame_param_ = param;

  return true;
}

void Renderer::UpdateRect(SDL_Rect &texture_rect, SDL_Rect &render_rect) {
  texture_rect.x = 0, texture_rect.y = 0;
  texture_rect.w = current_frame_param_.width;
  texture_rect.h = current_frame_param_.height;
  int w = 0, h = 0;
  SDL_GetWindowSize(window_, &w, &h);
  if (texture_rect.w*1.0/w > texture_rect.h*1.0/h) {
    render_rect.x = 0;
    render_rect.y = 0;
    render_rect.w = w;
    render_rect.h = texture_rect.h*w*1.0/texture_rect.w;
  } else {
    render_rect.x = 0;
    render_rect.y = 0;
    render_rect.h = h;
    render_rect.w = texture_rect.w*h*1.0/texture_rect.h;
  }

  render_rect.h *= 2;
  render_rect.w *= 2;

  // 居中显示
  render_rect.x = (w*2 - render_rect.w)/2;
  render_rect.y = (h*2 - render_rect.h)/2;

  // LOG_ERROR << "texture: " << texture_rect.x << ", " << texture_rect.y << ", " << texture_rect.w << ", " <<texture_rect.h;
  // LOG_ERROR << "render: " << render_rect.x << ", " << render_rect.y << ", " << render_rect.w << ", " <<render_rect.h;
}

bool Renderer::ConvertToYUV420(Frame &frame) {
  if (frame.param.pix_fmt == AV_PIX_FMT_YUV420P) {
    return true;
  }

  if (!ResetSwsContext(frame.param.width, frame.param.height, frame.param.pix_fmt)) {
    LOG_ERROR << "ResetSwsContext failed";
    return false;
  }

  uint8_t *data_ptr[4] = {
    &frame.data[0],
    (&frame.data[0]) + frame.data_offset[1],
    (&frame.data[0]) + frame.data_offset[2],
    (&frame.data[0]) + frame.data_offset[3]
  };

  int height = sws_scale(sws_context_,
      (const uint8_t * const*)data_ptr, frame.param.linesize, 0, frame.param.height,
      sws_pixel_data_.data, sws_pixel_data_.linesize);

  memcpy(frame.param.linesize, sws_pixel_data_.linesize, sizeof(sws_pixel_data_.linesize));
  frame.param.pix_fmt = AV_PIX_FMT_YUV420P;
  frame.param.height = height;
  auto begin = sws_pixel_data_.data[0];
  auto end = sws_pixel_data_.data[0] + sws_pixel_data_.data_size;
  frame.data = std::vector<uint8_t>(begin, end);

  return true;
}

void Renderer::Render() {
  SDL_Rect texture_rect;
  SDL_Rect render_rect;

  while (is_alive_) {
    Frame frame;
    if (!submit_queue_.TimedGet(&frame, std::chrono::milliseconds(100))) {
      continue;
    }

    if (!ConvertToYUV420(frame)) {
      LOG_ERROR << "ConvertToYUV420 failed";
      break;
    }

    if (!ResetTexture(frame.param)) {
      LOG_ERROR << "ResetTexture failed";
      break;
    }

    int64_t delay_time = delay_time_calculator_(frame.param.pts);
    if (delay_time > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(delay_time));
    }

    //LOG_ERROR << "pts: " << frame.param.pts << ", delay: " << delay_time
    //  << ", height: " << frame.param.height
    //  << ", width: " << frame.param.width
    //  << ", linesize: " << frame.param.linesize
    //  << ", pix_fmt: " << av_get_pix_fmt_name(frame.param.pix_fmt);

    UpdateRect(texture_rect, render_rect);

    //static std::ofstream outfile ("test.yuv", std::ofstream::binary);
    //outfile.write((const char *)&frame.data[0], frame.data.size());
    //outfile.flush();

    SDL_UpdateTexture(texture_, nullptr, &frame.data[0], frame.param.linesize[0]);
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
  window_ = SDL_CreateWindow(nullptr, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      800/*width*/, 800/*height*/, SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE);
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

  SDL_AddEventWatch(&SDLEvnetFilter, reinterpret_cast<void *>(this));
}

int Renderer::SDLEvnetFilterInternal(SDL_Event *event) {
  if (event && event->type == SDL_WINDOWEVENT && event->window.windowID == SDL_GetWindowID(window_)) {
    if (SDL_WINDOWEVENT_CLOSE == event->window.event) {
      LOG_ERROR << "window: " << event->window.windowID << " receives close event";
      Kill();
    }
  }
  // LOG_ERROR << "type: " << event->type  << ", id: " << event->window.windowID << ", event: " << int(event->window.event);
  return 0;
}

Renderer::~Renderer() {
  Kill();
  SDL_DelEventWatch(&SDLEvnetFilter, reinterpret_cast<void *>(this));
  if (sws_context_) {
    sws_freeContext(sws_context_);
    sws_context_ = nullptr;
  }
}

}
}
