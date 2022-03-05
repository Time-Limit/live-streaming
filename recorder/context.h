#pragma once

#include "recorder/base.h"
#include "util/renderer.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#include <SDL2/SDL.h>

#include <vector>
#include <memory>

namespace live {
namespace recorder {

class Context {
  // 输入视频参数
  std::vector<InputVideoParam> input_video_params_;
  std::vector<std::unique_ptr<Input>> inputs_;
  std::vector<std::unique_ptr<live::util::Renderer>> renderers_;

 public:
  Context();
  bool IsAlive() {
    for (auto &u : renderers_) {
      if (u->IsAlive()) {
        return true;
      }
    }
    return false;
  }
};

}
}
