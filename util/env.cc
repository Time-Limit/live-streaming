#include "util/env.h"
#include "util/util.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

#include <SDL2/SDL.h>

namespace live {
namespace util {

const std::thread::id main_thread_id = std::this_thread::get_id();

void WaitSDLEventUntilCheckerReturnFalse(std::function<bool()> checker) {
  SDL_Event windowEvent;
  while (checker()) {
    SDL_WaitEventTimeout(&windowEvent, 333);
  }
}

}  // namespace util
}  // namespace live

namespace {

struct SDLLibGuarder {
  SDLLibGuarder() {
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
      std::string reason = std::string("SDL_Init failed, ") + SDL_GetError();
      LOG_ERROR << reason;
      throw reason;
    }
  }
  ~SDLLibGuarder() {
    SDL_Quit();
  }
};

static SDLLibGuarder sdl_lib_guarder;

struct FFmpegLibGuarder {
  FFmpegLibGuarder() {
    avdevice_register_all();
  }
  ~FFmpegLibGuarder() {}
};

static FFmpegLibGuarder ffmpeg_lib_guarder;

}  // namespace
