#include "recorder/context.h"
#include "recorder/args.h"

#include <vector>
#include <string>

namespace live {
namespace recorder {

Context::Context() {
  // 初始化 FFmpeg
  avdevice_register_all();

  const std::string &input_video_params = FLAGS_input_video;
  const bool enable_debug_render = FLAGS_enable_debug_renderer;

  // 初始化输入视频参数
  for (int pos = 0, next = 0; pos < input_video_params.size();) {
    next = input_video_params.find(';', pos);
    input_video_params_.emplace_back(input_video_params.substr(pos, next-pos));
    if (next != std::string::npos) {
      pos = next+1;
    } else {
      break;
    }
  }

  if (input_video_params_.empty() && input_video_params_.size() > 2) {
    throw std::string("input_video_params is invalid, it should be less equal than 2 and greater than 0");
  }

  for (const auto &param : input_video_params_) {
    renderers_.emplace_back(std::make_unique<::live::util::Renderer>(
          [] (uint64_t point) -> uint64_t { return 0; }
    ));
    inputs_videos_.emplace_back(std::make_unique<Input>(param,
      [this, pr = renderers_.back().get()] (Input::Frame &&frame) {
        pr->Submit(std::move(frame));
      }
    ));
  }

  const std::string &input_audio_params = FLAGS_input_audio;
  // 初始化输入音频参数
  for (int pos = 0, next = 0; pos < input_audio_params.size();) {
    next = input_audio_params.find(';', pos);
    input_audio_params_.emplace_back(input_audio_params.substr(pos, next-pos));
    if (next != std::string::npos) {
      pos = next+1;
    } else {
      break;
    }
  }

  if (input_audio_params_.empty() && input_audio_params_.size() > 2) {
    throw std::string("input_audio_params is invalid, it should be less equal than 2 and greater than 0");
  }

  for (const auto &param : input_audio_params_) {
    speakers_.emplace_back(std::make_unique<util::Speaker>());
    inputs_audios_.emplace_back(std::make_unique<Input>(param,
      [this, ps = speakers_.back().get()] (Input::Sample &&sample) {
        // static std::ofstream ofile ("test.pcm",std::ofstream::binary);
        // ofile.write(reinterpret_cast<const char *>(&sample.data[0]), sample.data.size());
        // ofile.flush();
        ps->Submit(std::move(sample));
      }
    ));
  }
}

}
}
