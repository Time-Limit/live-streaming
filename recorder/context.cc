#include "recorder/context.h"
#include "recorder/args.h"

#include <vector>
#include <string>
#include <sstream>

namespace live {
namespace recorder {

Context::Context() {
  // 初始化 FFmpeg
  avdevice_register_all();

  const std::string &input_video_params = FLAGS_input_video;
  const bool enable_debug_render = FLAGS_enable_debug_renderer;

  // 初始化输入视频参数
  for (int pos = 0, next = 0; pos < input_video_params.size();) {
    next = input_video_params.find(',', pos);
    input_video_params_.emplace_back(input_video_params.substr(pos, next-pos));
    if (next != std::string::npos) {
      pos = next+1;
    } else {
      break;
    }
  }

  //if (input_video_params_.empty() && input_video_params_.size() > 2) {
  //  throw std::string("input_video_params is invalid, it should be less equal than 2 and greater than 0");
  //}
  
  if (input_video_params_.empty() && input_video_params_.size() == 2) {
    throw std::string("input_video_params is invalid, it should be equal to two");
  }

  std::stringstream main_desc, minor_desc;

#define WriteDescription(desc, input) \
  {\
    auto context = input->GetCodecContext(); \
    auto stream = input->GetStream(); \
    desc << "video_size=" << context->width << "x" << context->height \
      << ":pix_fmt=" << context->pix_fmt \
      << ":time_base=" << stream->time_base.num << "/" << stream->time_base.den \
      << ":pixel_aspect=" << context->sample_aspect_ratio.num << "/" << context->sample_aspect_ratio.den; \
    LOG_ERROR << desc.str();\
  }

  input_videos_.emplace_back(std::make_unique<Input>(input_video_params_[0], [this](Input::Frame &&frame) {
    static std::string input = "main";
    filter->Sumbit(input, std::move(frame));
  }));

  input_videos_.emplace_back(std::make_unique<Input>(input_video_params_[1], [this](Input::Frame &&frame) {
    static std::string input = "minor";
    filter->Sumbit(input, std::move(frame));
  }));

  WriteDescription(main_desc, input_videos_[0]);
  WriteDescription(minor_desc, input_videos_[1]);

#undef WriteDescription

  filter_.reset(new util::Filter(
    {
      {std::string("main"), main_desc.str()},
      {std::string("minor"), minor_desc.str()}
    },
    {
      {std::string("ouput"), std::string()}
    },
    "[minor]scale=w=160:h=320[scaled_minor],[scaled_minor]vflip[fliped_scaled_minor],[main][filped_scaled_minor]overlay[output]"
  ));

  //for (const auto &param : input_video_params_) {
  //  live::util::Renderer *debug_render = nullptr;
  //  if (enable_debug_render) {
  //    debug_renderers_.emplace_back(std::make_unique<::live::util::Renderer>(
  //          [] (uint64_t point) -> uint64_t { return 0; }
  //    ));
  //    debug_render = debug_renderers_.back().get();
  //  }
  //  input_videos_.emplace_back(std::make_unique<Input>(param,
  //    [this, ptr = debug_render] (Input::Frame &&frame) {
  //      if (ptr) {
  //        ptr->Submit(std::move(frame));
  //      }
  //    }
  //  ));
  //}

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

  if (input_audio_params_.size() != 1) {
    throw std::string("input_audio_params is invalid, it must have one input audio parameter");
  }

  for (const auto &param : input_audio_params_) {
    inputs_audios_.emplace_back(std::make_unique<Input>(param,
      [this] (Input::Sample &&sample) {
        // static std::ofstream ofile ("test.pcm",std::ofstream::binary);
        // ofile.write(reinterpret_cast<const char *>(&sample.data[0]), sample.data.size());
        // ofile.flush();
      }
    ));
  }
}

}
}
