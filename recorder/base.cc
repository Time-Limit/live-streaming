#include "recorder/base.h"
#include "recorder/args.h"

extern "C" {
#include "libavutil/samplefmt.h"
}

namespace live {
namespace recorder {

bool Input::InitDecodeContext(enum AVMediaType type) {
  int ret = -1;
  ret = av_find_best_stream(format_context_, type, -1, -1, nullptr, 0);
  if (ret < 0) {
    LOG_ERROR << "could not find " << av_get_media_type_string(type) << " stream";
    return false;
  }

  int stream_index = ret;
  AVStream *st = format_context_->streams[stream_index];

  /* find decoder for the stream */
  const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
  if (!dec) {
    LOG_ERROR << "failed to find " << av_get_media_type_string(type) << " codec";
    return false;
  }
  
  /* Allocate a codec context for the decoder */
  dec_ctx_ = avcodec_alloc_context3(dec);
  if (!dec_ctx_) {
    LOG_ERROR << "failed to allocate the " << av_get_media_type_string(type) << " codec context";
    return false;
  }
  
  /* Copy codec parameters from input stream to output codec context */
  if ((ret = avcodec_parameters_to_context(dec_ctx_, st->codecpar)) < 0) {
    LOG_ERROR << "failed to copy " << av_get_media_type_string(type) << " codec parameters to decoder context";
    return false;
  }
  
  /* Init the decoders */
  if ((ret = avcodec_open2(dec_ctx_, dec, NULL)) < 0) {
    LOG_ERROR << "failed to open " << av_get_media_type_string(type) << " codec";
    return false;
  }
  stream_idx_ = stream_index;
  stream_ = st;
  LOG_ERROR << "type: " << av_get_media_type_string(type) << ", stream_index: " << stream_idx_
    << ", stream_address: " << stream_
    << ", codec: " << dec->name
    << ", sample rate: " << stream_->codecpar->sample_rate << ", channel: " << stream_->codecpar->channels
    << ", sample format in stream: " << stream_->codecpar->format
    << ", sample format in decode context: " << dec_ctx_->sample_fmt;
  return true;
}

Input::Input(const InputVideoParam &param, FrameReceiver receiver)
  : input_type_(InputType::VIDEO), input_video_param_(param), frame_receiver_(std::move(receiver)) {
  if (!frame_receiver_) {
    throw std::string("receiver is not callable");
  }

  format_context_ = avformat_alloc_context();
  input_ = av_find_input_format(FLAGS_input_format.c_str());

  AVDictionary *dict = nullptr;
  av_dict_set(&dict, "video_size", param.input_w_x_h.c_str(), 0);
  av_dict_set(&dict, "framerate", std::to_string(param.framerate).c_str(), 0);
  av_dict_set(&dict, "pixel_format", param.pix_fmt.c_str(), 0);
  int ret = avformat_open_input(&format_context_, param.url.c_str(), input_, &dict);

  av_dict_free(&dict);

  if (ret < 0) {
    throw ::std::string("avformat_open_input failed") + av_err2str(ret);
  }

  if (!InitDecodeContext(AVMEDIA_TYPE_VIDEO)) {
    throw std::string("InitDecodeContext failed");
  }
}

Input::Input(const InputAudioParam &param, SampleReceiver receiver)
  : input_type_(InputType::AUDIO), input_audio_param_(param), sample_receiver_(std::move(receiver)) {
  if (!sample_receiver_) {
    throw std::string("receiver is not callable");
  }

  format_context_ = avformat_alloc_context();
  input_ = av_find_input_format(FLAGS_input_format.c_str());

  int ret = avformat_open_input(&format_context_, param.url.c_str(), input_, nullptr);

  if (ret < 0) {
    throw ::std::string("avformat_open_input failed") + av_err2str(ret);
  }

  if (!InitDecodeContext(AVMEDIA_TYPE_AUDIO)) {
    throw std::string("InitDecodeContext failed");
  }

}

void Input::Decode() {
  std::vector<Frame> frames;
  std::vector<Sample> samples;
  while (is_alive_) {
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
      LOG_ERROR << "could not allocate packet";
      break;
    }
    int ret = 0;
    if ((ret = av_read_frame(format_context_, packet)) < 0) {
      if (ret == AVERROR(EAGAIN)) { continue; }
      LOG_ERROR << "av_read_frame failed, ret: " << ret << ", err: " << av_err2str(ret);
      break;
    }
    if (packet->stream_index == stream_idx_) {
      if (input_type_ == InputType::VIDEO) {
        frames.resize(0);
        if (!decoder_.DecodeVideoPacket(stream_, dec_ctx_, packet, &frames)) {
          LOG_ERROR << "decode failed, url: " << input_video_param_.url; 
          break;
        }
        for (auto &f : frames) {
          frame_receiver_(std::move(f));
        }
      } else if (input_type_ == InputType::AUDIO) {
        samples.resize(0);
        if (!decoder_.DecodeAudioPacket(stream_, dec_ctx_, packet, &samples)) {
          LOG_ERROR << "decode failed, url: " << input_audio_param_.url; 
          break;
        }
        for (auto &s : samples) {
          sample_receiver_(std::move(s));
        }
      } else {
        av_packet_free(&packet);
        throw std::string("undefined input type");
      }
    } else {
      av_packet_free(&packet);
    }
  }
  if (is_alive_) {
    LOG_ERROR << "decoding thread exits, url: " << input_audio_param_.url << input_video_param_.url;
  }
  is_alive_ = false;
}

Input::~Input() {
  is_alive_ = false;
  LOG_ERROR << "input is closing, url: " << input_audio_param_.url << input_video_param_.url;
  if (decoder_future_.valid()) {
    decoder_future_.wait();
  }
  avcodec_free_context(&dec_ctx_);
  avformat_close_input(&format_context_);
  LOG_ERROR << "input is closed, url: " << input_audio_param_.url << input_video_param_.url;
}

InputVideoParam::InputVideoParam(std::string param) {
  if (param.size() >= 1024) {
    throw std::string("param is too long");
  }
  char url_buf[1024] = {0};
  char pix_fmt_buf[1024] = {0};
  char input_w_x_h_buf[1024] = {0};
  for (auto &c : param) {
    (c == ':') ? (c = '\n') : c;
  }
  int ret = sscanf(param.c_str(), "%s %s %s %d",
      url_buf,
      input_w_x_h_buf,
      pix_fmt_buf,
      &framerate);
  if (ret != 4) {
    throw std::string("invalid param");
  }
  url = url_buf;
  pix_fmt = pix_fmt_buf;
  input_w_x_h = input_w_x_h_buf;
  LOG_ERROR << "sscanf ret: " << ret
    << ", url: " << url
    << ", input_w_x_h: " << input_w_x_h
    << ", pix_fmt: " << pix_fmt
    << ", framerate: " << framerate;
}

bool Input::Run() {
  if (is_alive_) {
    LOG_ERROR << "already killed";
    return false;
  }

  if (decoder_future_.valid()) {
    return true;
  }

  decoder_future_ = std::move(std::async(std::launch::async, &Input::Decode, this));

  return true;
}

void Input::Kill() {
  is_alive_ = false;
}

}
}
