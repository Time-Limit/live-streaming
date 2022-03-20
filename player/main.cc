#include "player/args.h"
#include "player/context.h"
#include "util/util.h"
#include "util/speaker.h"
#include "util/decoder.h"
#include "util/renderer.h"
#include "util/env.h"

using namespace live::player;
using namespace live::util;

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

  Renderer renderer([&context](const AVFrameWrapper &frame) {
    return context.CalcDelayTimeInMicroSecond(frame);
  });
  auto decode_video_packet_func = [&context, &video_packet_q_, &renderer, &quit_flag] () {
    Decoder decoder;
    AVPacket *packet = nullptr;
    std::vector<AVFrameWrapper> frames;
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

  Speaker speaker([&context](const AVFrameWrapper &sample){
    context.UpdatePlayingTimeInterval(sample);
    //context.UpdatePlayingTimeInterval(sample.GetSideData().pts, sample.GetSideData().duration);
  });
  auto decode_audio_packet_func = [&context, &audio_packet_q_, &speaker, &quit_flag] () {
    Decoder decoder;
    AVPacket *packet = nullptr;
    std::vector<AVFrameWrapper> samples;
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

  live::util::WaitSDLEventUntilCheckerReturnFalse(
      [p_qf = &quit_flag, p_r = &renderer, p_s = &speaker] () {
        return !(*p_qf) && p_r->IsAlive() && p_s->IsAlive();
      }
  );

  LOG_ERROR << "exiting...";

  quit_flag = true;

  speaker.Kill();
  renderer.Kill();

  audio_decoder_future.wait();
  LOG_ERROR << "audio decoder exits";
  video_decoder_future.wait();
  LOG_ERROR << "video decoder exits";
  read_future.wait();
  LOG_ERROR << "reading thread exits";

  return 0;
}
