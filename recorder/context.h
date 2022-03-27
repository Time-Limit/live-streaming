#pragma once

#include "recorder/base.h"
#include "util/renderer.h"
#include "util/speaker.h"
#include "util/filter.h"
#include "util/muxer.h"

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
  // 调试用的Renderer 和 Speaker
  std::vector<std::unique_ptr<live::util::Renderer>> debug_renderers_;
  std::vector<std::unique_ptr<live::util::Speaker>> debug_speakers_;

  // Muxing
  std::unique_ptr<util::Muxer> muxer_;

  // 滤镜
  std::unique_ptr<util::Filter> filter_;

  // 输入视频相关变量
  std::vector<InputVideoParam> input_video_params_;
  std::vector<std::unique_ptr<Input>> input_videos_;
  
  // 输入音频相关变量
  std::vector<InputAudioParam> input_audio_params_;
  std::vector<std::unique_ptr<Input>> input_audios_;


 public:
  Context();
  bool IsAlive() {
    if (!filter_->IsAlive()) {
      return false;
    }
    for (auto &input : input_videos_) {
      if (input->IsAlive()) {
        return true;
      }
    }
    for (auto &input : input_audios_) {
      if (input->IsAlive()) {
        return true;
      }
    }
    return false;
  }
};

}
}
