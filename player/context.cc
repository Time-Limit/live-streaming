#include "player/context.h"

namespace live {
namespace player {

bool Context::InitLocalFileReader(const std::string &path) {
  try {
    reader_.reset(new LocalFileReader(path));
  } catch (const std::string &err) {
    LOG_ERROR << err;
    return false;
  }
  return true;
}

int Context::ReadPacketForAVIOContext(void *opaque, uint8_t *buf, int buf_size) {
  Context *context = reinterpret_cast<Context *>(opaque);
  if (!context->reader_) {
    return AVERROR_EOF;
  }
  return context->reader_->Read(buf, buf_size);
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
      0, this, &Context::ReadPacketForAVIOContext, nullptr, nullptr);

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

Context::~Context() {
  avcodec_free_context(&video_dec_ctx_);
  avcodec_free_context(&audio_dec_ctx_);
  avformat_close_input(&format_context_);
  av_packet_free(&packet_);
  av_frame_free(&frame_);
  av_free(video_dst_data_[0]);

  if (avio_ctx_) {
    av_freep(&avio_ctx_->buffer);
    avio_context_free(&avio_ctx_);
  }
}

}
}
