#include "util/muxer.h"
#include "util/util.h"

namespace live {
namespace util {

static bool InitStream(OutputStream *ost, AVFormatContext *oc, const AVCodec **codec,
      enum AVCodecID codec_id, const MuxerParam &mp) {
  
  AVCodecContext *c = nullptr;
  int i = 0;

  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) {
    LOG_ERROR << "not found encoder for " << avcodec_get_name(codec_id);
    return false;
  }

  if(!(ost->packet = av_packet_alloc())) {
    LOG_ERROR << "alloc AVPacket failed";
    return false;
  }

  if (!(ost->st = avformat_new_stream(oc, NULL))) {
    LOG_ERROR << "alloc AVStream failed";
    return false;
  }

  ost->st->id = oc->nb_streams-1;

  if (!(c = avcodec_alloc_context3(*codec))) {
    LOG_ERROR << "alooc AVCodecContext failed";
    return false;
  }

  ost->enc = c;
  switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO: {
      c->codec_id = codec_id;
      c->sample_fmt = mp.audio_sample_format;
      // c->bit_rate = mp.audio_bit_rate;
      c->sample_rate = mp.audio_sample_rate;
      if ((*codec)->supported_samplerates) {
        c->sample_rate = (*codec)->supported_samplerates[0];
        for (i = 0; (*codec)->supported_samplerates[i]; i++) {
          if ((*codec)->supported_samplerates[i] == mp.audio_sample_rate)
            c->sample_rate = mp.audio_sample_rate;
        }
      }
      c->channel_layout = mp.audio_channel_layout;
      if ((*codec)->channel_layouts) {
        c->channel_layout = (*codec)->channel_layouts[0];
        for (i = 0; (*codec)->channel_layouts[i]; i++) {
          if ((*codec)->channel_layouts[i] == mp.audio_channel_layout)
            c->channel_layout = mp.audio_channel_layout;
        }
      }
      c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
      ost->st->time_base = c->time_base = mp.audio_time_base;
      break;
    }
    case AVMEDIA_TYPE_VIDEO: {
      c->codec_id = codec_id;
      // c->bit_rate = mp.video_bit_rate;
      c->width = mp.video_width;
      c->height = mp.video_height;
      ost->st->time_base = c->time_base = mp.video_time_base;
      c->pix_fmt = mp.video_pix_fmt;
      c->gop_size = 16;
      break;
    }
    default: {
      LOG_ERROR << "not handler this codec type " << (*codec)->type;
      return false;
    }
  }
  
  //if ((*codec)->pix_fmts) {
  //  for (int i = 0; (*codec)->pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
  //    LOG_ERROR << av_get_pix_fmt_name((*codec)->pix_fmts[i]);
  //  }
  //}

  //if ((*codec)->sample_fmts) {
  //  for (int i = 0; (*codec)->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++) {
  //    LOG_ERROR << av_get_sample_fmt_name((*codec)->sample_fmts[i]);
  //  }
  //}

  int ret = avcodec_open2(c, *codec, nullptr);
  if (ret < 0) {
    LOG_ERROR << "avcodec_open2 failed, error: " << av_err2str(ret);
    return false;
  }

  ret = avcodec_parameters_from_context(ost->st->codecpar, c);
  if (ret < 0) {
    LOG_ERROR << "avcodec_parameters_from_context failed, error: " << av_err2str(ret);
  }

  if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  return true;
}

Muxer::Muxer(const MuxerParam &mp) {
  muxer_param_ = mp;
  avformat_alloc_output_context2(&format_context_, nullptr, nullptr, filename.c_str());
  if (!format_context_) {
    throw std::string("alloc format context failed");
  }

  output_format_ = format_context_->oformat;

  if (output_format_->video_codec == AV_CODEC_ID_NONE) {
    throw std::string("not found video codec");
  }
  if (!InitStream(&video_st_, format_context_, &video_codec_, output_format_->video_codec, muxer_param_)) {
    throw std::string ("init video stream failed");
  }

  if (output_format_->audio_codec == AV_CODEC_ID_NONE) {
    throw std::string("not found audio codec");
  }
  if (!InitStream(&audio_st_, format_context_, &audio_codec_, output_format_->audio_codec, muxer_param_)) {
    throw std::string("init audio stream failed");
  }

  av_dump_format(format_context_, 0, filename.c_str(), 1);

  is_alive_ = true;
  muxing_future_ = std::async(std::launch::async, [this] () {
    while (is_alive_) {
      AVFrameWrapper frame;
      if (!queue_.TimedGet(&frame, std::chrono::milliseconds(100))) {
        continue;
      }
    }
  });
}

Muxer::~Muxer() {
  is_alive_ = false;
  if (muxing_future_.valid()) {
    muxing_future_.wait();
  }

  avcodec_free_context(&video_st_.enc);
  av_frame_free(&video_st_.frame);
  av_packet_free(&video_st_.packet);

  avcodec_free_context(&audio_st_.enc);
  av_frame_free(&audio_st_.frame);
  av_packet_free(&audio_st_.packet);

  avformat_free_context(format_context_);
}

}
}
