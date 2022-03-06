#include "util/decoder.h"
#include "util/base.h"

namespace live {
namespace util {

Decoder::Decoder() {
  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    throw std::string("alloc AVFrame failed");
  }
}
Decoder::~Decoder() {
  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
}

bool Decoder::PixelData::Reset(int w, int h, AVPixelFormat fmt) {
  if (w == width && h == height && pix_fmt == fmt) {
    return true;
  }
  this->~PixelData();
  PixelData();
  data_size = av_image_alloc(data, linesize, w, h, fmt, 1);
  if (data_size < 0) {
    LOG_ERROR << "av_image_alloc failed";
    return false;
  }
  w = width, h = height, pix_fmt = fmt;
  return true;
}

bool Decoder::ResetSwsContext(int w, int h, AVPixelFormat fmt) {
  if (sws_pixel_data_.height == h && sws_pixel_data_.width == w && sws_pixel_data_.pix_fmt == fmt) {
    return true;
  }
  if (sws_context_) {
    sws_freeContext(sws_context_);
    sws_context_ = nullptr;
  }
  sws_context_ = sws_getContext(w, h, fmt, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!sws_context_) {
    LOG_ERROR << "sws_getContext failed";
    return false;
  }
  if (!sws_pixel_data_.Reset(w, h, AV_PIX_FMT_YUV420P)) {
    return false;
  }
  return true;
}

bool Decoder::DecodeVideoPacket(const AVStream *stream, AVCodecContext *ctx,
    const AVPacket *pkt, std::vector<Frame> *frames) {
  auto callback = [this, pkt, stream, ctx, frames] (const AVFrame *av_frame) -> bool {

    int width = ctx->width;
    int height = ctx->height;
    AVPixelFormat pix_fmt = ctx->pix_fmt;
    if (!pixel_data_.Reset(width, height, pix_fmt)) {
      LOG_ERROR << "alloc pixel data buffer failed";
      return false;
    }
    
    av_image_copy(pixel_data_.data, pixel_data_.linesize,
        (const uint8_t **)(av_frame->data), av_frame->linesize,
        pix_fmt, width, height);

    std::vector<uint8_t> frame_data;
    int32_t linesize = pixel_data_.linesize[0];

    // 将其他格式转换为 YUV420P，方便 SDL 处理
    if (pix_fmt != AV_PIX_FMT_YUV420P) {
      if (!ResetSwsContext(width, height, pix_fmt)) {
        LOG_ERROR << "ResetSwsContext failed";
        return false;
      }

      height = sws_scale(sws_context_,
          (const uint8_t * const*)pixel_data_.data, pixel_data_.linesize, 0, height,
          sws_pixel_data_.data, sws_pixel_data_.linesize);

      frame_data.insert(frame_data.end(),
          sws_pixel_data_.data[0], sws_pixel_data_.data[0] + sws_pixel_data_.data_size);

      // overwrite linesize and pix_fmt
      linesize = sws_pixel_data_.linesize[0];
      pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
      frame_data.insert(frame_data.end(), pixel_data_.data[0], pixel_data_.data[0] + pixel_data_.data_size);
    }

    frames->emplace_back();
    Frame &frame = frames->back();
    frame.data = std::move(frame_data);
    frame.param.height = height;
    frame.param.width = width;
    frame.param.pix_fmt = pix_fmt;
    frame.param.linesize = linesize;
    frame.param.pts = av_frame->pts * 1000000L * stream->time_base.num / stream->time_base.den;

    // LOG_ERROR << "video, pts in AVFrame: " << av_frame->pts << ", pts in FrameParam: " << frame.param.pts
    //   << ", stream->time_base: " << stream->time_base.num << "/" << stream->time_base.den;

    return true;
  };
  return Packet2AVFrame(ctx, pkt, callback);
}

AVSampleFormat ExtractPCMData(AVSampleFormat sample_format, int sample_number, int channel_number,
    const AVFrame *frame, std::vector<uint8_t> &sample_data) {
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
        sample_data.insert(sample_data.end(),
            frame->extended_data[0], frame->extended_data[0] + frame->linesize[0]);
        return sample_format;
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

        return dict[sample_format];
      }
    default:
      {
        LOG_ERROR << "invalid sample format: " << sample_format;
        return AV_SAMPLE_FMT_NONE;
      }
  }
  return AV_SAMPLE_FMT_NONE;
}

bool Decoder::DecodeAudioPacket(const AVStream *stream, AVCodecContext *ctx,
    const AVPacket *pkt, std::vector<Sample> *samples) {

  auto callback = [pkt, stream, samples, ctx] (const AVFrame *av_frame) -> bool {
    // enum AVSampleFormat 定义参见 FFmpeg/libavutil/samplefmt.h
    AVSampleFormat sample_format = AVSampleFormat(ctx->sample_fmt);
    int sample_number = av_frame->nb_samples;
    int channel_number = stream->codecpar->channels;
    int sample_rate = stream->codecpar->sample_rate;
    std::vector<uint8_t> sample_data;

    // 将 PCM 数据从 av_frame 复制至 sample_data
    sample_format = ExtractPCMData(sample_format, sample_number, channel_number, av_frame, sample_data);
    if (sample_format == AV_SAMPLE_FMT_NONE) {
      LOG_ERROR << "extract pcm data failed";
      return false;
    }

    // 写入容器中，供外部使用
    samples->emplace_back();
    Sample &sample = samples->back();
    sample.data = std::move(sample_data);
    sample.param.channel_number = channel_number;
    sample.param.sample_rate = sample_rate;
    sample.param.sample_number = sample_number;
    sample.param.sample_format = sample_format;
    sample.param.pts = av_frame->pts * 1000000L * stream->time_base.num / stream->time_base.den;
    sample.param.duration = sample_number * 1000000L * stream->time_base.num / stream->time_base.den;
    
    //LOG_ERROR << "audio, pts in AVFrame: " << av_frame->pts << ", pts in FrameParam: " << sample.param.pts
    //  << ", duration: " << sample.param.duration
    //  << ", stream->time_base: " << stream->time_base.num << "/" << stream->time_base.den;

    return true;
  };
  return Packet2AVFrame(ctx, pkt, callback);
}

bool Decoder::Packet2AVFrame(AVCodecContext *ctx, const AVPacket *pkt, const FrameDataExtractor &extractor) {

  // submit the packet to the decoder
  int ret = avcodec_send_packet(ctx, pkt);
  if (ret < 0) {
    LOG_ERROR << "error submitting a packet for decoding, " <<  av_err2str(ret);
    return false;
  }

  // get all the available frames from the decoder
  while (ret >= 0) {
    ret = avcodec_receive_frame(ctx, av_frame_);
    if (ret < 0) {
      // those two return values are special and mean there is no output
      // frame available, but there were no errors during decoding
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        return true;
      }
      LOG_ERROR << "error during decoding " << av_err2str(ret);
      return false;
    }
    if (!extractor(av_frame_)) {
      LOG_ERROR << "the result of callback is wrong";
      return false;
    }
    av_frame_unref(av_frame_);
  }
  return true;
}

}
}
