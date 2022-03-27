#include "util/muxer.h"
#include "util/util.h"

namespace live {
namespace util {

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
  AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

  printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
      av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
      av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
      av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
      pkt->stream_index);
}

static bool InitStream(OutputStream *ost, AVFormatContext *oc, const AVCodec **codec,
      enum AVCodecID codec_id, MuxerParam *mp) {
  
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
      c->sample_fmt = mp->audio_sample_format;
      if ((*codec)->sample_fmts) {
        c->sample_fmt = (*codec)->sample_fmts[0];
        for (i = 0; (*codec)->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++) {
          if ((*codec)->sample_fmts[i] == mp->audio_sample_format) {
            c->sample_fmt = (*codec)->sample_fmts[i];
            break;
          }
        }

        LOG_ERROR << "change sample format from " << av_get_sample_fmt_name(mp->audio_sample_format)
          << " to " << av_get_sample_fmt_name(c->sample_fmt);

        mp->audio_sample_format = c->sample_fmt;
      }

      c->sample_rate = mp->audio_sample_rate;
      if ((*codec)->supported_samplerates) {
        c->sample_rate = (*codec)->supported_samplerates[0];
        for (i = 0; (*codec)->supported_samplerates[i]; i++) {
          if ((*codec)->supported_samplerates[i] == mp->audio_sample_rate) {
            c->sample_rate = mp->audio_sample_rate;
            break;
          }
        }

        LOG_ERROR << "change sample rate from " << mp->audio_sample_rate << " to " << c->sample_rate;

        mp->audio_sample_rate = c->sample_rate;
      }

      c->channel_layout = mp->audio_channel_layout;
      if ((*codec)->channel_layouts) {
        c->channel_layout = (*codec)->channel_layouts[0];
        for (i = 0; (*codec)->channel_layouts[i]; i++) {
          if ((*codec)->channel_layouts[i] == mp->audio_channel_layout) {
            c->channel_layout = mp->audio_channel_layout;
            break;
          }
        }

        LOG_ERROR << "change channel layout from " << mp->audio_channel_layout << " to " << c->channel_layout;

        mp->audio_channel_layout = c->channel_layout;
      }
    
      c->channels = av_get_channel_layout_nb_channels(c->channel_layout);

      c->bit_rate =
        av_get_bytes_per_sample(mp->audio_sample_format) * av_get_channel_layout_nb_channels(AVSampleFormat(mp->audio_channel_layout)) * mp->audio_sample_rate;

      LOG_ERROR << "set bit rate to " << c->bit_rate;

      ost->st->time_base = c->time_base = mp->audio_time_base;
      break;
    }
    case AVMEDIA_TYPE_VIDEO: {
      c->codec_id = codec_id;
      c->width = mp->video_width;
      c->height = mp->video_height;
      ost->st->time_base = c->time_base = mp->video_time_base;
      c->pix_fmt = mp->video_pix_fmt;
      if ((*codec)->pix_fmts) {
        c->pix_fmt = (*codec)->pix_fmts[0];
        for (int i = 0; (*codec)->pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
          if ((*codec)->pix_fmts[i] == mp->video_pix_fmt) {
            c->pix_fmt = mp->video_pix_fmt;
            break;
          }
        }
        LOG_ERROR << "change pix fmt from " << av_get_pix_fmt_name(mp->video_pix_fmt)
          <<" to " <<  av_get_pix_fmt_name(c->pix_fmt);
        mp->video_pix_fmt = c->pix_fmt;
      }

      c->gop_size = 16;

      c->bit_rate = mp->video_height * mp->video_width * 3 * 60;

      break;
    }
    default: {
      LOG_ERROR << "not handler this codec type " << (*codec)->type;
      return false;
    }
  }

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
  if (!InitStream(&video_st_, format_context_, &video_codec_, output_format_->video_codec, &muxer_param_)) {
    throw std::string ("init video stream failed");
  }

  if (output_format_->audio_codec == AV_CODEC_ID_NONE) {
    throw std::string("not found audio codec");
  }
  if (!InitStream(&audio_st_, format_context_, &audio_codec_, output_format_->audio_codec, &muxer_param_)) {
    throw std::string("init audio stream failed");
  }

  av_dump_format(format_context_, 0, filename.c_str(), 1);

  is_alive_ = true;
  muxing_future_ = std::async(std::launch::async, [this] () {
    auto exit_func = [this] () {
      is_alive_ = false;
    };
    ScopeGuard<decltype(exit_func)> guard(std::move(exit_func));

    int ret = 0;
    if (!(output_format_->flags & AVFMT_NOFILE)) {
      ret = avio_open(&format_context_->pb, filename.c_str(), AVIO_FLAG_WRITE);
      if (ret < 0) {
        LOG_ERROR << "open " << filename << " failed, error: " << av_err2str(ret);
        return;
      }
    }
  
    /* Write the stream header, if any. */
    ret = avformat_write_header(format_context_, nullptr);
    if (ret < 0) {
      LOG_ERROR << "write header failed, error: " << av_err2str(ret);
      return;
    }

    while (is_alive_) {
      AVFrameWrapper frame;
      if (!queue_.TimedGet(&frame, std::chrono::milliseconds(100))) {
        continue;
      }

      OutputStream *os = nullptr;

      if (frame->channel_layout) {
        if (muxer_param_.audio_sample_rate != frame->sample_rate
            || muxer_param_.audio_channel_layout != frame->channel_layout
            || muxer_param_.audio_sample_format != frame->format) {
          if (!audio_resample_helper_.Resample(frame, muxer_param_.audio_sample_rate, muxer_param_.audio_channel_layout, muxer_param_.audio_sample_format)) {
            LOG_ERROR << "audio frame resample failed";
            continue;
          }
        }
        os = &audio_st_;
      } else if (frame->width && frame->height) {
        if (muxer_param_.video_height != frame->height
            || muxer_param_.video_width != frame->width
            || muxer_param_.video_pix_fmt != frame->format) {
          if (!video_scale_helper_.Scale(frame, muxer_param_.video_width, muxer_param_.video_height, muxer_param_.video_pix_fmt)) {
            LOG_ERROR << "video frame scale failed";
            continue;
          }
        }
        os = &video_st_;
      } else {
        LOG_ERROR << "WTF type ?";
        continue;
      }

       ret = avcodec_send_frame(os->enc, frame.GetRawPtr());
       if (ret < 0) {
         LOG_ERROR << "send frame failed, error: " << av_err2str(ret);
         return;
       }

       while (ret >= 0) {
         ret = avcodec_receive_packet(os->enc, os->packet);
         if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
           break;
         else if (ret < 0) {
           LOG_ERROR << "receive packet failed, error: " << av_err2str(ret);
           return;
         }

         /* rescale output packet timestamp values from codec to stream timebase */
         av_packet_rescale_ts(os->packet, os->enc->time_base, os->st->time_base);
         os->packet->stream_index = os->st->index;

         /* Write the compressed frame to the media file. */
         // log_packet(format_context_, os->packet);
         ret = av_interleaved_write_frame(format_context_, os->packet);
         /* pkt is now blank (av_interleaved_write_frame() takes ownership of
          * its contents and resets pkt), so that no unreferencing is necessary.
          * This would be different if one used av_write_frame(). */
         if (ret < 0) {
           LOG_ERROR << "write frame failed, error: " << av_err2str(ret);
           return;
         }
       }
    }

    av_write_trailer(format_context_);
  });
}

Muxer::~Muxer() {
  is_alive_ = false;
  if (muxing_future_.valid()) {
    muxing_future_.wait();
  }

  avcodec_free_context(&video_st_.enc);
  av_packet_free(&video_st_.packet);

  avcodec_free_context(&audio_st_.enc);
  av_packet_free(&audio_st_.packet);

  if (!(output_format_->flags & AVFMT_NOFILE)) {
    avio_closep(&format_context_->pb);
  }

  avformat_free_context(format_context_);
}

}
}
