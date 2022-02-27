#include "player/args.h"
#include "player/context.h"
#include "util/util.h"
#include "player/speaker.h"
#include "player/decoder.h"
#include "player/renderer.h"

using namespace live::player;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  Context context(live::player::FLAGS_local_file, true);

  live::util::Queue<AVPacket *> audio_packet_q_;
  live::util::Queue<AVPacket *> video_packet_q_;
  
  bool quit_flag = false;
  bool read_finish_flag = false;

  auto read_func = [&read_finish_flag, &quit_flag, &context, &audio_packet_q_, &video_packet_q_] () mutable {
    while (!quit_flag) {
      AVPacket *packet = av_packet_alloc();
      if (!packet) {
        LOG_ERROR << "could not allocate packet";
        break;
      }
      int ret = 0;
      if ((ret = av_read_frame(context.GetFormatContext(), packet)) < 0) {
        LOG_ERROR << "av_read_frame failed, ret: " << ret << ", err: " << av_err2str(ret);
        break;
      }
      if (context.IsVideoPacket(packet)) {
        video_packet_q_.Put(std::move(packet));
      } else if (context.IsAudioPacket(packet)) {
        audio_packet_q_.Put(std::move(packet));
      } else {
        av_packet_free(&packet);
      }
    }
    video_packet_q_.Put(nullptr);
    audio_packet_q_.Put(nullptr);
    read_finish_flag = true;
  };
  auto read_future = std::async(std::launch::async, std::move(read_func));

  Renderer renderer(context.GetWindow(), [&context](int64_t time_point) {
    return context.CalcDelayTimeInMicroSecond(time_point);
  });
  auto decode_video_packet_func = [&context, &video_packet_q_, &renderer, &quit_flag] () {
    Decoder decoder;
    AVPacket *packet = nullptr;
    std::vector<Frame> frames;
    auto stream = context.GetVideoStream();
    auto decode_context = context.GetVideoCodecContext();

    while (!quit_flag) {
      if (!video_packet_q_.TimedGet(&packet, std::chrono::milliseconds(100))) {
        continue;
      }
      frames.resize(0);
      if (!decoder.DecodeVideoPacket(stream, decode_context, packet, &frames)) {
        LOG_ERROR << "decode failed";
        return;
      }
      for (auto &frame : frames) {
        renderer.Submit(std::move(frame));
      }
      av_packet_free(&packet);
      packet = nullptr;
    }
  };
  auto video_decoder_future = std::async(std::launch::async, std::move(decode_video_packet_func));

  Speaker speaker([&context](const Sample *sample){
    context.UpdatePlayingTimeInterval(sample->param.pts, sample->param.duration);
  });
  auto decode_audio_packet_func = [&context, &audio_packet_q_, &speaker, &quit_flag] () {
    Decoder decoder;
    AVPacket *packet = nullptr;
    std::vector<Sample> samples;
    auto stream = context.GetAudioStream();
    auto decode_context = context.GetAudioCodecContext();

    while (!quit_flag) {
      if (!audio_packet_q_.TimedGet(&packet, std::chrono::milliseconds(100))) {
        continue;
      }
      samples.resize(0);
      if (!decoder.DecodeAudioPacket(stream, decode_context, packet, &samples)) {
        LOG_ERROR << "decode failed";
        return;
      }
      for (auto &sample : samples) {
        speaker.Submit(std::move(sample));
      }
      av_packet_free(&packet);
      packet = nullptr;
    }
  };
  auto audio_decoder_future = std::async(std::launch::async, std::move(decode_audio_packet_func));

  while (!renderer.IsStop() && !speaker.IsStop() && !quit_flag) {
    SDL_Event windowEvent;
    if (read_finish_flag
        && audio_packet_q_.Size() == 0 && video_packet_q_.Size() == 0
        && !speaker.HasPendingData() && !renderer.HasPendingData()) {
      quit_flag = true;
      continue;
    }
    if (!SDL_WaitEventTimeout(&windowEvent, 100)) {
      continue;
    }
    switch (windowEvent.type) {
      case SDL_QUIT:
        {
          quit_flag = true;
          break;
        }
      case SDL_WINDOWEVENT:
        {
          const auto &window = windowEvent.window;
          if (window.event == SDL_WINDOWEVENT_RESIZED) {
            LOG_ERROR << "window resized, width: " << window.data1 << ", height: " << window.data2;
            renderer.SetWindowSize(window.data1, window.data2);
          }
          break;
        }
      default: {}
    }
  }
  LOG_ERROR << "waiting event loop is broken";

  quit_flag = true;

  speaker.Stop();
  renderer.Stop();

  audio_decoder_future.wait();
  LOG_ERROR << "audio decoder exits";
  video_decoder_future.wait();
  LOG_ERROR << "video decoder exits";
  read_future.wait();
  LOG_ERROR << "reading thread exits";

  return 0;
}
