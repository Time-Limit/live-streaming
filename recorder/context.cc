#include "recorder/context.h"
#include "recorder/args.h"

#include <sstream>
#include <string>
#include <vector>

namespace live {
namespace recorder {

Context::Context() {
  const std::string& input_video_params = FLAGS_input_video;
  const bool enable_debug_renderer = FLAGS_enable_debug_renderer;
  const bool enable_debug_speaker = FLAGS_enable_debug_speaker;
  const bool enable_output_pts_info = FLAGS_enable_output_pts_info;

  // 初始化输入视频参数
  for (int pos = 0, next = 0; pos < input_video_params.size();) {
    next = input_video_params.find(',', pos);
    input_video_params_.emplace_back(
        input_video_params.substr(pos, next - pos));
    if (next != std::string::npos) {
      pos = next + 1;
    } else {
      break;
    }
  }

  if (input_video_params_.empty() || input_video_params_.size() != 2) {
    throw std::string(
        "input_video_params is invalid, it should be equal to two");
  }

  std::stringstream main_desc, minor_desc;

#define WriteDescription(desc, input)                                   \
  {                                                                     \
    auto context = input->GetCodecContext();                            \
    auto stream = input->GetStream();                                   \
    desc << "video_size=" << context->width << "x" << context->height   \
         << ":pix_fmt=" << context->pix_fmt                             \
         << ":time_base=" << stream->time_base.num << "/"               \
         << stream->time_base.den                                       \
         << ":pixel_aspect=" << context->sample_aspect_ratio.num << "/" \
         << context->sample_aspect_ratio.den;                           \
    LOG_ERROR << desc.str();                                            \
  }

  input_videos_.emplace_back(std::make_unique<Input>(
      input_video_params_[0], [this](util::AVFrameWrapper&& frame) {
        static std::string input = "main";
        filter_->Submit(input, std::move(frame));
      }));

  input_videos_.emplace_back(std::make_unique<Input>(
      input_video_params_[1], [this](util::AVFrameWrapper&& frame) {
        static std::string input = "minor";
        filter_->Submit(input, std::move(frame));
      }));

  WriteDescription(main_desc, input_videos_[0]);
  WriteDescription(minor_desc, input_videos_[1]);

#undef WriteDescription

  live::util::Renderer* debug_render = nullptr;
  if (enable_debug_renderer) {
    debug_renderers_.emplace_back(std::make_unique<::live::util::Renderer>(
        [](const util::AVFrameWrapper&) -> int64_t { return 0; }));
    debug_render = debug_renderers_.back().get();
  }

  filter_.reset(new util::Filter(
      {{std::string("main"), main_desc.str()},
       {std::string("minor"), minor_desc.str()}},
      std::string("output"), std::string(),
      "[minor]scale=w=480:h=-1[scaled_minor],[scaled_minor]hflip[fliped_scaled_"
      "minor],[main][fliped_scaled_minor]overlay[output]",
      [debug_render, enable_output_pts_info,
       this](const util::AVFrameWrapper& wrapper) {
        if (enable_output_pts_info) {
          LOG_ERROR << "video: " << wrapper->pts
                    << ", time_base: " << wrapper->time_base.num << '/'
                    << wrapper->time_base.den;
        }
        if (debug_render) {
          auto tmp = wrapper;
          debug_render->Submit(std::move(tmp));
        }
        {
          auto tmp = wrapper;
          muxer_->Submit(std::move(tmp));
        }
      }));

  const std::string& input_audio_params = FLAGS_input_audio;
  // 初始化输入音频参数
  for (int pos = 0, next = 0; pos < input_audio_params.size();) {
    next = input_audio_params.find(';', pos);
    input_audio_params_.emplace_back(
        input_audio_params.substr(pos, next - pos));
    if (next != std::string::npos) {
      pos = next + 1;
    } else {
      break;
    }
  }

  if (input_audio_params_.size() != 1) {
    throw std::string(
        "input_audio_params is invalid, it must have one input audio "
        "parameter");
  }

  live::util::Speaker* debug_speaker = nullptr;

  if (enable_debug_speaker) {
    debug_speakers_.emplace_back(std::make_unique<::live::util::Speaker>());
    debug_speaker = debug_speakers_.back().get();
  }

  for (const auto& param : input_audio_params_) {
    input_audios_.emplace_back(std::make_unique<Input>(
        param, [this, debug_speaker,
                enable_output_pts_info](util::AVFrameWrapper&& sample) {
          if (enable_output_pts_info) {
            LOG_ERROR << "audio: " << sample->pts
                      << ", time_base: " << sample->time_base.num << '/'
                      << sample->time_base.den;
          }
          if (debug_speaker) {
            auto tmp = sample;
            debug_speaker->Submit(std::move(tmp));
          }
          {
            auto tmp = sample;
            muxer_->Submit(std::move(tmp));
          }
        }));
  }

  util::MuxerParam muxer_param;
  muxer_param.audio_sample_rate =
      input_audios_.back()->GetAgreedUponParam().sample_rate;
  muxer_param.audio_channel_layout =
      input_audios_.back()->GetAgreedUponParam().channel_layout;
  muxer_param.audio_time_base =
      input_audios_.back()->GetAgreedUponParam().time_base;
  muxer_param.audio_sample_format =
      input_audios_.back()->GetAgreedUponParam().sample_format;

  muxer_param.video_width = filter_->GetOutputParam().width;
  muxer_param.video_height = filter_->GetOutputParam().height;
  muxer_param.video_pix_fmt = filter_->GetOutputParam().pix_fmt;
  muxer_param.video_time_base = filter_->GetOutputParam().time_base;

  muxer_.reset(new util::Muxer(muxer_param));

  for (auto& input : input_videos_) {
    input->Run();
  }

  for (auto& input : input_audios_) {
    input->Run();
  }
}

}  // namespace recorder
}  // namespace live
