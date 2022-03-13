#pragma once

#include "recorder/base.h"
#include "util/renderer.h"
#include "util/speaker.h"
#include "util/filter.h"

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
  // 输入视频相关变量
  std::vector<InputVideoParam> input_video_params_;
  std::vector<std::unique_ptr<Input>> input_videos_;
  std::vector<std::unique_ptr<live::util::Renderer>> debug_renderers_;
  
  // 输入音频相关变量
  std::vector<InputAudioParam> input_audio_params_;
  std::vector<std::unique_ptr<Input>> inputs_audios_;

  // 滤镜
  std::unique_ptr<util::Filter> filter_;

 public:
  Context();
  bool IsAlive() {
    for (auto &u : debug_renderers_) {
      if (u->IsAlive()) {
        return true;
      }
    }
    return false;
  }
};

}
}
