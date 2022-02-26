#include "player/context.h"
#include "player/args.h"

#include <chrono>
#include <thread>
#include <future>

extern "C" {
#include <libswscale/swscale.h>
}

namespace live {
namespace player {

#if 0

bool Context::InitLocalFileReader(const std::string &path) {
  try {
    reader_.reset(new LocalFileReader(path));
  } catch (const std::string &err) {
    LOG_ERROR << err;
    return false;
  }
  return true;
}

int64_t Context::SeekForAVIOContext(void *opaque, int64_t offset, int whence) {
  Context *context = reinterpret_cast<Context *>(opaque);
  if (!context->reader_) {
    context->FatalErrorOccurred();
    return -1;
  }
  int64_t ret = context->reader_->Seek(offset, whence);
  if (ret < 0) {
    LOG_ERROR << "seek failed, offset: " << offset << ", whence: " << whence;
    // context->FatalErrorOccurred();
  }
  return ret;
}

int Context::ReadForAVIOContext(void *opaque, uint8_t *buf, int buf_size) {
  Context *context = reinterpret_cast<Context *>(opaque);
  if (!context->reader_) {
    context->FatalErrorOccurred();
    return AVERROR_EOF;
  }
  auto ret = context->reader_->Read(buf, buf_size);
  if (ret == AVERROR_EOF) {
    context->FatalErrorOccurred();
  }
  return ret;
}

bool Context::InitDecodeContext(int *index, AVCodecContext **dec_ctx, enum AVMediaType type) {
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
  *dec_ctx = avcodec_alloc_context3(dec);
  if (!*dec_ctx) {
    LOG_ERROR << "failed to allocate the " << av_get_media_type_string(type) << " codec context";
    return false;
  }
  
  /* Copy codec parameters from input stream to output codec context */
  if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
    LOG_ERROR << "failed to copy " << av_get_media_type_string(type) << " codec parameters to decoder context";
    return false;
  }
  
  /* Init the decoders */
  if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
    LOG_ERROR << "failed to open " << av_get_media_type_string(type) << " codec";
    return false;
  }
  *index = stream_index;
  return true;
}

bool Context::InitDecoder(const std::string &format) {
  int ret = -1;
  if (!(format_context_ = avformat_alloc_context())) {
    LOG_ERROR << "alloc AVFormatContext failed";
    return false;
  }
  avio_ctx_buffer_size_ = 4096;
  if (!(avio_ctx_buffer_ = reinterpret_cast<uint8_t *>(av_malloc(avio_ctx_buffer_size_)))) {
    LOG_ERROR << "alloc avio_ctx_buffer failed";
    return false;
  }

  avio_ctx_ = avio_alloc_context(avio_ctx_buffer_, avio_ctx_buffer_size_,
      0, this, &Context::ReadForAVIOContext, nullptr, &Context::SeekForAVIOContext);

  if (!avio_ctx_) {
    LOG_ERROR << "alloc AVIOContext failed";
    return false;
  }

  format_context_->pb = avio_ctx_;

  const AVInputFormat *input_format = av_find_input_format(format.c_str());

  ret = avformat_open_input(&format_context_, nullptr, input_format, nullptr);
  if (ret == -1) {
    LOG_ERROR << "avformat_open_input failed";
    return false;
  }

  ret = avformat_find_stream_info(format_context_, nullptr);
  if (ret == -1) {
    LOG_ERROR << "avformat_find_stream_info failed";
    return false;
  }

  av_dump_format(format_context_, 0, format.c_str(), 0);

  InitDecodeContext(&video_stream_idx_, &video_dec_ctx_, AVMEDIA_TYPE_VIDEO);
  InitDecodeContext(&audio_stream_idx_, &audio_dec_ctx_, AVMEDIA_TYPE_AUDIO);

  if (video_stream_idx_ == -1 && audio_stream_idx_ == -1) {
    LOG_ERROR << "could not find audio or video stream in the input";
    return false;
  }

  frame_ = av_frame_alloc();
  if (!frame_) {
    LOG_ERROR << "could not allocate frame";
    return false;
  }
  packet_ = av_packet_alloc();
  if (!packet_) {
    LOG_ERROR << "could not allocate packet";
    return false;
  }

  return true;
}

bool Context::GetAudioParamAndDataMessages(AVFrame *frame, std::vector<Message> *msgs) {

  LOG_ERROR << "audio: " << av_ts2timestr(frame->pts, &audio_dec_ctx_->time_base);
  
  // size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
  
  // printf("audio_frame n:%d nb_samples:%d pts:%s\n",
  //       audio_frame_count++, frame->nb_samples,
  //       av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
  
  /* Write the raw audio data samples of the first plane. This works
   * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
   * most audio decoders output planar audio, which uses a separate
   * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
   * In other words, this code will write only the first audio channel
   * in these cases.
   * You should use libswresample or libavfilter to convert the frame
   * to packed data. */

  const AVStream *audio_stream = format_context_->streams[audio_stream_idx_];

  // enum AVSampleFormat 定义参见 FFmpeg/libavutil/samplefmt.h
  AVSampleFormat sample_format = AVSampleFormat(audio_stream->codecpar->format);
  int sample_number = frame->nb_samples;
  int channel_number = audio_stream->codecpar->channels;
  int sample_rate = audio_stream->codecpar->sample_rate;
  std::vector<uint8_t> sample_data;

  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_S64:
    {
      // AvFrame::extended_data 参见 libavutil/frame.h:369
      // AvFrame::linesize 参见 libavutil/frame.h:353
      sample_data.insert(sample_data.end(), frame->extended_data[0], frame->extended_data[0] + frame->linesize[0]);
      break;
    }
    case AV_SAMPLE_FMT_U8P:
    case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64P:
    {
      static const AVSampleFormat dict[AV_SAMPLE_FMT_NB] = {
        AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE,
        AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_DBL,
        AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_S64
      };

      int sample_size = av_get_bytes_per_sample(sample_format);

      for (int i = 0; i < sample_number; i++) {
        for (int j = 0; j < channel_number; j++) {
          auto begin = frame->extended_data[j] + i * sample_size;
          auto end = frame->extended_data[j] + (i + 1) * sample_size;
          sample_data.insert(sample_data.end(), begin, end);
        }
      }

      //LOG_INFO << "convert sample format, "
      //  << av_get_sample_fmt_name(sample_format) << " to " << av_get_sample_fmt_name(dict[sample_format]);

      sample_format = dict[sample_format];
      break;
    }
    default: {
      LOG_ERROR << "invalid sample format: " << sample_format;
      return false;
    }
  }

  {
    // copy audio param from format_context
    msgs->emplace_back(Message{Message::Type::AUDIO_PARAM, std::vector<uint8_t>(sizeof(AudioParam))});
    AudioParam *param = reinterpret_cast<AudioParam *>(&msgs->back().data[0]);
    param->sample_format = sample_format;
    param->channels = channel_number;
    param->sample_rate = sample_rate;
  }

  {
    msgs->emplace_back(Message{Message::Type::DATA, std::move(sample_data)});
  }

  return true;
}

bool Context::GetVideoParamAndDataMessages(AVFrame *frame, std::vector<Message> *msgs) {
  LOG_ERROR << "video: " << av_ts2timestr(frame->pts, &video_dec_ctx_->time_base);
  const uint8_t VIDEO_DST_SIZE = 4;
  uint8_t *video_dst_data[VIDEO_DST_SIZE] = {nullptr};
  int video_dst_linesize[VIDEO_DST_SIZE] = {0};
  int width = video_dec_ctx_->width;
  int height = video_dec_ctx_->height;
  AVPixelFormat pix_fmt = video_dec_ctx_->pix_fmt;
  int video_dst_bufsize = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
  if (video_dst_bufsize < 0) {
    LOG_ERROR << "alloc raw video buffer failed";
  }

  av_image_copy(video_dst_data, video_dst_linesize,
      (const uint8_t **)(frame->data), frame->linesize,
      pix_fmt, width, height);

  std::vector<uint8_t> frame_data;
  int32_t linesize = video_dst_linesize[0];

  // 将其他格式转换为 YUV420P，方便 SDL 处理
  if (pix_fmt != AV_PIX_FMT_YUV420P) {
    auto free_func = [](SwsContext *ctx) -> void { sws_freeContext(ctx); };
    std::unique_ptr<SwsContext, decltype(free_func)> sws_ctx(sws_getContext(width, height, pix_fmt,
          width, height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr), free_func);

    if (!sws_ctx) {
      LOG_ERROR << "alloc sws context failed";
      return false;
    }

    uint8_t *dst_data[VIDEO_DST_SIZE] = {nullptr};
    int dst_linesize[VIDEO_DST_SIZE] = {0};
    int dst_bufsize = av_image_alloc(dst_data, dst_linesize, width, height, AV_PIX_FMT_YUV420P, 1);
    if (dst_bufsize < 0) {
      LOG_ERROR << "alloc sws buffer failed";
    }

    sws_scale(sws_ctx.get(),
        (const uint8_t * const*)video_dst_data, video_dst_linesize, 0, height,
        dst_data, dst_linesize);

    frame_data.insert(frame_data.end(), dst_data[0], dst_data[0] + dst_bufsize);
    linesize = dst_linesize[0];
    pix_fmt = AV_PIX_FMT_YUV420P;
    
    av_free(dst_data[0]);
  } else {
    frame_data.insert(frame_data.end(), video_dst_data[0], video_dst_data[0] + video_dst_bufsize);
  }

  av_free(video_dst_data[0]);

  {
    msgs->emplace_back(Message{Message::Type::VIDEO_PARAM, std::vector<uint8_t>(sizeof(VideoParam))});
    VideoParam *param = reinterpret_cast<VideoParam *>(&msgs->back().data[0]);
    param->height = height;
    param->width = width;
    param->pix_fmt = pix_fmt;
    param->linewidth = linesize;
    if (video_dec_ctx_->framerate.num == 0 && video_dec_ctx_->framerate.den == 1) {
      // 可用 AVStream::avg_frame_rate 代替
      AVStream *st = format_context_->streams[video_stream_idx_];
      param->frame_rate = st->avg_frame_rate.num*1.0f/st->avg_frame_rate.den;
    } else {
      param->frame_rate = video_dec_ctx_->framerate.num*1.0f/video_dec_ctx_->framerate.den;
    }
  }

  {
    msgs->emplace_back(Message{Message::Type::DATA, std::move(frame_data)});
  }

  return true;
}

bool Context::DecodePacket(enum AVMediaType type, const AVPacket *pkt, std::vector<Message> *msgs) {
  AVCodecContext *dec = nullptr;
  if (type == AVMEDIA_TYPE_VIDEO) {
    dec = video_dec_ctx_;
  } else if (type == AVMEDIA_TYPE_AUDIO) {
    dec = audio_dec_ctx_;
  } else {
    LOG_ERROR << "not support this type " << type;
    return false;
  }

  int ret = 0;

  // submit the packet to the decoder
  ret = avcodec_send_packet(dec, pkt);
  if (ret < 0) {
    LOG_ERROR << "error submitting a packet for decoding " <<  av_err2str(ret);
    return false;
  }

  // get all the available frames from the decoder
  while (ret >= 0) {
    ret = avcodec_receive_frame(dec, frame_);
    if (ret < 0) {
      // those two return values are special and mean there is no output
      // frame available, but there were no errors during decoding
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        return true;
      }
      LOG_ERROR << "error during decoding " << av_err2str(ret);
      return false;
    }

    // write the frame data to output file
    if (type == AVMEDIA_TYPE_VIDEO) {
      if (!GetVideoParamAndDataMessages(frame_, msgs)) {
        return false;
      }
    } else if (type == AVMEDIA_TYPE_AUDIO) {
      if (!GetAudioParamAndDataMessages(frame_, msgs)) {
        return false;
      }
    }
    av_frame_unref(frame_);
  }
  return true;
}

void Context::Decode() {
  // TODO 应该根据每帧的播放时长来定
  int sleep_ms = 3;
  size_t video_queue_size_limit = 100;
  size_t audio_queue_size_limit = 100;

  while (!quit_flag_ && !read_finish_ && !has_fatal_error_) {
    bool audio_is_enough = true, video_is_enough = true;
    if (video_stream_idx_ != -1 && video_queue_.Size() < video_queue_size_limit) {
      video_is_enough = false;
    }
    if (audio_stream_idx_ != -1 && audio_queue_.Size() <  audio_queue_size_limit) {
      audio_is_enough = false;
    }

    while ((!video_is_enough || !audio_is_enough) && av_read_frame(format_context_, packet_) >= 0) {
      if (packet_->stream_index == video_stream_idx_) {
        std::vector<Message> msgs;
        if (!DecodePacket(AVMEDIA_TYPE_VIDEO, packet_, &msgs)) {
          FatalErrorOccurred();
          break;
        }
        if (video_queue_.Put(std::move(msgs)) >= video_queue_size_limit) {
          video_is_enough = true;
        }
      } else if (packet_->stream_index == audio_stream_idx_) {
        std::vector<Message> msgs;
        if (!DecodePacket(AVMEDIA_TYPE_AUDIO, packet_, &msgs)) {
          FatalErrorOccurred();
          break;
        }
        if (audio_queue_.Put(std::move(msgs)) >= audio_queue_size_limit) {
          audio_is_enough = true;
        }
      }
      av_packet_unref(packet_);
    }

    if (read_finish_) {
      if (video_dec_ctx_) {
        std::vector<Message> msgs;
        DecodePacket(AVMEDIA_TYPE_VIDEO, nullptr, &msgs);
        video_queue_.Put(std::move(msgs));
      }
      if (audio_dec_ctx_) {
        std::vector<Message> msgs;
        DecodePacket(AVMEDIA_TYPE_AUDIO, nullptr, &msgs);
        audio_queue_.Put(std::move(msgs));
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
  }
}

void Context::SDLAudioDeviceCallbackInternal(Uint8 *stream, int len) {
  SDL_memset(stream, 0, len);

  std::vector<uint8_t> next_package;
  while (pcm_data_buffer_.size() < len && pcm_data_queue_.TryToGet(&next_package)) {
    pcm_data_buffer_.insert(pcm_data_buffer_.end(), next_package.begin(), next_package.end());
  }
  if (len > pcm_data_buffer_.size()) {
    len = pcm_data_buffer_.size();
  }
  if (len == 0) {
    return;
  }
  SDL_MixAudioFormat(stream, &pcm_data_buffer_[0], desired_audio_spec_.format, len, SDL_MIX_MAXVOLUME);
  auto dst = &pcm_data_buffer_[0];
  auto src = &pcm_data_buffer_[0] + len;
  auto copy_size = pcm_data_buffer_.size() - len;
  memcpy(dst, src, copy_size);
  pcm_data_buffer_.resize(copy_size);
}

bool Context::OpenAudioDevice(const AudioParam &param) {
  if (audio_device_id_ != -1) {
    LOG_ERROR << "audio device has already opened, audio_device_id: " << audio_device_id_;
    return false;
  }

  // 转换参数
  desired_audio_spec_.freq = param.sample_rate;
  desired_audio_spec_.channels = param.channels;
  desired_audio_spec_.samples = 1<<10;
  // 转换 FFmpeg 的 sample_format 至 SDL 的
  switch (param.sample_format) {
    case AV_SAMPLE_FMT_U8: desired_audio_spec_.format = AUDIO_U8; break;
    case AV_SAMPLE_FMT_S16: desired_audio_spec_.format = AUDIO_S16SYS; break;
    case AV_SAMPLE_FMT_S32: desired_audio_spec_.format = AUDIO_S32SYS; break;
    case AV_SAMPLE_FMT_FLT: desired_audio_spec_.format = AUDIO_F32SYS; break;
    // case AV_SAMPLE_FMT_DBL:
    // case AV_SAMPLE_FMT_S64:
    default: {
      LOG_ERROR << "not handled this FFmpeg sample format: " << param.sample_format
        << ", " << av_get_sample_fmt_name(param.sample_format);
      return false;
    }
  }

  desired_audio_spec_.callback = &Context::SDLAudioDeviceCallback;
  desired_audio_spec_.userdata = this;

  LOG_INFO << "freq: " << desired_audio_spec_.freq
    << ", channels: " << int(desired_audio_spec_.channels)
    << ", samples: " << desired_audio_spec_.samples
    << ", format: " << desired_audio_spec_.format;

  auto device_id = SDL_OpenAudioDevice(nullptr, 0, &desired_audio_spec_,
      &obtained_audio_spec_, SDL_AUDIO_ALLOW_ANY_CHANGE);

  LOG_INFO << "freq: " << obtained_audio_spec_.freq
    << ", channels: " << int(obtained_audio_spec_.channels)
    << ", samples: " << obtained_audio_spec_.samples
    << ", format: " << obtained_audio_spec_.format;

  if (device_id < 2) {
    LOG_ERROR << "SDL_OpenAudioDevice failed, ret: " << device_id << ", msg: " << SDL_GetError();
    return false;
  }

  audio_device_id_ = device_id;

  pcm_data_queue_.Clear();
  pcm_data_buffer_.resize(0);

  current_audio_param_ = param;

  SDL_PauseAudioDevice(audio_device_id_, 0);

  LOG_INFO << "open audio device success, device id " << audio_device_id_;
  return true;
}

bool Context::CloseAudioDevice() {
  if (audio_device_id_ != -1) {
    SDL_CloseAudioDevice(audio_device_id_);
    LOG_ERROR << "close audio device, id: " << audio_device_id_;
    audio_device_id_ = -1;
  }
  current_audio_param_ = AudioParam();
  return true;
}

void Context::PlayPCM() {
  std::this_thread::sleep_for(std::chrono::seconds(10));
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_AUDIO)) {
    LOG_ERROR << "SDL_Init failed, " << SDL_GetError();
    FatalErrorOccurred();
    return;
  }

  while (!quit_flag_) {
    Message msg;
    if (!audio_queue_.TimedGet(&msg, std::chrono::milliseconds(100))) {
      continue;
    }
    switch (msg.type) {
      case Message::Type::AUDIO_PARAM: {
        const AudioParam *param = reinterpret_cast<const AudioParam *>(&msg.data[0]);
        if (audio_device_id_ == -1) {
          if (!OpenAudioDevice(*param)) {
            LOG_ERROR << "OpenAudioDevice failed";
            FatalErrorOccurred();
            return;
          }
        } else {
          if (!param->IsSame(current_audio_param_)) {
            if (!CloseAudioDevice()) {
              LOG_ERROR << "CloseAudioDevice failed";
              FatalErrorOccurred();
              return;
            }
            if (!OpenAudioDevice(*param)) {
              LOG_ERROR << "OpenAudioDevice failed";
              FatalErrorOccurred();
              return;
            }
          }
        }
        break;
      }
      case Message::Type::DATA: {
        pcm_data_queue_.Put(std::move(msg.data));
        break;
      }
      default: {
        LOG_ERROR << "not handled this type " << msg.type;
        FatalErrorOccurred();
        break;
      }
    }
  }
}

bool Context::CreateTexture(const VideoParam &param) {
  LOG_ERROR << "width: " << param.width
    << ", height: " << param.height
    << ", linewidth: " << param.linewidth
    << ", pix_fmt: " << av_get_pix_fmt_name(param.pix_fmt)
    << ", frame size: " << param.height *  param.linewidth
    << ", frame rate: " << param.frame_rate;

  SDL_PixelFormatEnum sdl_pixel_format = SDL_PIXELFORMAT_UNKNOWN;

  switch (param.pix_fmt) {
    case AV_PIX_FMT_YUV420P: {
      sdl_pixel_format = SDL_PIXELFORMAT_IYUV;
      break;
    }
    default: {
      LOG_ERROR << "not handled this format " << av_get_pix_fmt_name(param.pix_fmt);
      return false;
    }
  }

  texture_ = SDL_CreateTexture(render_,
      sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING,
      param.width, param.height);

  if (texture_ == nullptr) {
    LOG_ERROR << "create texture failed, " << SDL_GetError();
    DestroyWindow();
    return false;
  }

  current_video_param_ = param;

  return true;
}

bool Context::CreateWindow() {
  if (window_ != nullptr) {
    LOG_ERROR << "window has already created, window: " << window_;
    return false;
  }

  //window_ = SDL_CreateWindow(nullptr,
  //    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
  //    window_width, window_height,
  //    SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE);
  window_ = SDL_CreateWindow(nullptr,
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      FLAGS_window_width, FLAGS_window_height,
      SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE);

  window_size_.store(WindowSize{FLAGS_window_width, FLAGS_window_height});

  if (window_ == nullptr) {
    LOG_ERROR << "create window failed, " << SDL_GetError();
    return false;
  }

  render_ = SDL_CreateRenderer(window_, -1, 0);
  if (render_ == nullptr) {
    LOG_ERROR << "create render failed, " << SDL_GetError();
    DestroyWindow();
    return false;
  }

  LOG_ERROR << "create window success";

  return true;
}

bool Context::DestroyTexture() {
  SDL_DestroyTexture(texture_);
  texture_ = nullptr;
  return true;
}

bool Context::DestroyWindow() {
  DestroyTexture();

  SDL_DestroyRenderer(render_);
  render_ = nullptr;
  SDL_DestroyWindow(window_);
  window_ = nullptr;

  current_video_param_ = VideoParam();
  
  return true;
}

void Context::PlayRawVideo() {
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)) {
    LOG_ERROR << "SDL_Init failed, " << SDL_GetError();
    FatalErrorOccurred();
    return;
  }

  if (!CreateWindow()) {
    LOG_ERROR << "CreateWindow failed";
    FatalErrorOccurred();
    return;
  }

  auto future = std::async(std::launch::async, [this] () {
    std::unique_ptr<std::ofstream> video_file_ptr;
    video_file_ptr.reset(new std::ofstream("tmp.file", std::ofstream::binary));
    while (!quit_flag_) {
      Message msg;
      if (!video_queue_.TimedGet(&msg, std::chrono::milliseconds(100))) {
        continue;
      }
      SDL_Rect yuv_rect;
      switch (msg.type) {
        case Message::Type::VIDEO_PARAM: {
          const VideoParam *param = reinterpret_cast<VideoParam *>(&msg.data[0]);
          if (!texture_) {
            if (!CreateTexture(*param)) {
              LOG_ERROR << "CreateTexture failed";
              FatalErrorOccurred();
              return;
            }
          } else if (!param->IsSame(current_video_param_)) {
            if (DestroyTexture()) {
              LOG_ERROR << "DestroyTexture failed";
              FatalErrorOccurred();
              return;
            }
            if (!CreateTexture(*param)) {
              LOG_ERROR << "CreateTexture failed";
              FatalErrorOccurred();
              return;
            }
          }
          yuv_rect.x = 0;
          yuv_rect.y = 0;
          yuv_rect.w = param->width;
          yuv_rect.h = param->height;
          break;
        }
        case Message::Type::DATA: {
          int linewidth = current_video_param_.linewidth;
          int height = current_video_param_.height;
          int frame_size = linewidth * height;

          SDL_Rect render_rect;
          WindowSize cur_window_size = window_size_.load();
          if (yuv_rect.w*1.0/cur_window_size.w > yuv_rect.h*1.0/cur_window_size.h) {
            render_rect.x = 0;
            render_rect.y = 0;
            render_rect.w = cur_window_size.w;
            render_rect.h = yuv_rect.h*cur_window_size.w*1.0/yuv_rect.w;
          } else {
            render_rect.x = 0;
            render_rect.y = 0;
            render_rect.h = cur_window_size.h;
            render_rect.w = yuv_rect.w*cur_window_size.h*1.0/yuv_rect.h;
          }

          // TODO 不知为何，乘2后方能铺满窗口。
          render_rect.h *= 2;
          render_rect.w *= 2;

          // 居中显示
          render_rect.x = (cur_window_size.w*2 - render_rect.w)/2;
          render_rect.y = (cur_window_size.h*2 - render_rect.h)/2;

          // LOG_ERROR << "yuv: " << yuv_rect.w << ":" << yuv_rect.h << ":" << yuv_rect.x << ":" << yuv_rect.y;
          // LOG_ERROR << "ren: " << render_rect.w << ":" << render_rect.h << ":" << render_rect.x << ":" << render_rect.y;

          for (int cursor = 0; cursor + frame_size < msg.data.size() && !quit_flag_; ) {
            SDL_UpdateTexture(texture_, nullptr, &msg.data[cursor], linewidth);
            SDL_RenderClear(render_);
            SDL_RenderCopy(render_, texture_, &yuv_rect, &render_rect);
            SDL_RenderPresent(render_);
            int64_t sleep_micro_seconds = 100000/current_video_param_.frame_rate;
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_micro_seconds));
            cursor += frame_size;
          }
          break;
        }
        default: {
          LOG_ERROR << "not handled this type " << msg.type;
          FatalErrorOccurred();
          break;
        }
      }
    }
  });

  while(true && !quit_flag_) {
    SDL_Event windowEvent;
    SDL_WaitEvent(&windowEvent);
    switch (windowEvent.type) {
      case SDL_QUIT: {
        FatalErrorOccurred();
        break;
      }
      case SDL_WINDOWEVENT: {
        const auto &window = windowEvent.window;
        if (window.event == SDL_WINDOWEVENT_RESIZED) {
          LOG_ERROR << "window resized, width: " << window.data1 << ", height: " << window.data2;
          window_size_.store(WindowSize{window.data1, window.data2});
        }
        break;
      }
      default: {}
    }
  }

  future.wait();
}

void Context::Work() {
  auto decode_future = std::async(std::launch::async, &Context::Decode, this);

  auto play_pcm_future = std::async(std::launch::async, &Context::PlayPCM, this);

  PlayRawVideo();

  decode_future.wait();
  play_pcm_future.wait();
}

Context::~Context() {
  avcodec_free_context(&video_dec_ctx_);
  avcodec_free_context(&audio_dec_ctx_);
  avformat_close_input(&format_context_);
  av_packet_free(&packet_);
  av_frame_free(&frame_);

  if (avio_ctx_) {
    av_freep(&avio_ctx_->buffer);
    avio_context_free(&avio_ctx_);
  }

  CloseAudioDevice();
  DestroyWindow();
  SDL_Quit();
}

#endif

}
}
