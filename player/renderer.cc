#include "player/renderer.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
}

namespace live {
namespace player {

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
  WindowSize cur_window_size = window_size_.load();
  if (texture_rect.w*1.0/cur_window_size.w > texture_rect.h*1.0/cur_window_size.h) {
    render_rect.x = 0;
    render_rect.y = 0;
    render_rect.w = cur_window_size.w;
    render_rect.h = texture_rect.h*cur_window_size.w*1.0/texture_rect.w;
  } else {
    render_rect.x = 0;
    render_rect.y = 0;
    render_rect.h = cur_window_size.h;
    render_rect.w = texture_rect.w*cur_window_size.h*1.0/texture_rect.h;
  }

  // TODO 不知为何，乘2后方能铺满窗口。
  render_rect.h *= 2;
  render_rect.w *= 2;

  // 居中显示
  render_rect.x = (cur_window_size.w*2 - render_rect.w)/2;
  render_rect.y = (cur_window_size.h*2 - render_rect.h)/2;
}

void Renderer::Render() {
  SDL_Rect texture_rect;
  SDL_Rect render_rect;

  while (!is_stop_) {
    Frame frame;
    if (!submit_queue_.TimedGet(&frame, std::chrono::milliseconds(100))) {
      continue;
    }

    if (!ResetTexture(frame.param)) {
      LOG_ERROR << "ResetTexture failed";
      break;
    }

    UpdateRect(texture_rect, render_rect);

    SDL_UpdateTexture(texture_, nullptr, &frame.data[0], frame.param.linesize);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, &texture_rect, &render_rect);
    SDL_RenderPresent(renderer_);
  }
  is_stop_ = true;
}

Renderer::Renderer(SDL_Window *window) : render_future_(std::async(std::launch::async, &Renderer::Render, this)) {
  if (!window) {
    throw std::string("the pointer to SDL_Window is nullptr");
  }
  renderer_ = SDL_CreateRenderer(window, -1, 0);
  if (renderer_ == nullptr) {
    throw std::string("create render failed, ") + SDL_GetError();
  }
}

Renderer::~Renderer() {
  SDL_DestroyRenderer(renderer_);
  renderer_ = nullptr;

  SDL_DestroyTexture(texture_);
  texture_ = nullptr;
}

}
}
