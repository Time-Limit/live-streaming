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

bool Decoder::DecodeVideoPacket(const AVStream* stream, AVCodecContext* ctx,
                                const AVPacket* pkt,
                                std::vector<AVFrameWrapper>* frames) {
  auto callback = [this, stream, ctx, frames](const AVFrame* av_frame) -> bool {
    int width = ctx->width;
    int height = ctx->height;
    AVPixelFormat pix_fmt = ctx->pix_fmt;
    if (!pixel_data_.Reset(width, height, pix_fmt)) {
      LOG_ERROR << "alloc pixel data buffer failed";
      return false;
    }

    av_image_copy(pixel_data_.data, pixel_data_.linesize,
                  (const uint8_t**)(av_frame->data), av_frame->linesize,
                  pix_fmt, width, height);

    frames->emplace_back(av_frame);
    AVFrameWrapper& frame = frames->back();
    frame->time_base = stream->time_base;

    // LOG_ERROR << "video, pts in AVFrame: " << av_frame->pts << ", pts in
    // FrameParam: " << frame.param.pts
    //   << ", stream->time_base: " << stream->time_base.num << "/" <<
    //   stream->time_base.den;

    return true;
  };
  return Packet2AVFrame(ctx, pkt, callback);
}

AVSampleFormat ExtractPCMData(AVSampleFormat sample_format, int sample_number,
                              int channel_number, const AVFrame* frame,
                              std::vector<uint8_t>& sample_data) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_S64: {
      // AvFrame::extended_data 参见 libavutil/frame.h:369
      // AvFrame::linesize 参见 libavutil/frame.h:353
      sample_data.insert(sample_data.end(), frame->extended_data[0],
                         frame->extended_data[0] + frame->linesize[0]);
      return sample_format;
    }
    case AV_SAMPLE_FMT_U8P:
    case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64P: {
      static const AVSampleFormat dict[AV_SAMPLE_FMT_NB] = {
          AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE,
          AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_U8,
          AV_SAMPLE_FMT_S16,  AV_SAMPLE_FMT_S32,  AV_SAMPLE_FMT_FLT,
          AV_SAMPLE_FMT_DBL,  AV_SAMPLE_FMT_NONE, AV_SAMPLE_FMT_S64};

      int sample_size = av_get_bytes_per_sample(sample_format);

      for (int i = 0; i < sample_number; i++) {
        for (int j = 0; j < channel_number; j++) {
          auto begin = frame->extended_data[j] + i * sample_size;
          auto end = frame->extended_data[j] + (i + 1) * sample_size;
          sample_data.insert(sample_data.end(), begin, end);
        }
      }

      // LOG_INFO << "convert sample format, "
      //  << av_get_sample_fmt_name(sample_format) << " to " <<
      //  av_get_sample_fmt_name(dict[sample_format]);

      return dict[sample_format];
    }
    default: {
      LOG_ERROR << "invalid sample format: " << sample_format;
      return AV_SAMPLE_FMT_NONE;
    }
  }
  return AV_SAMPLE_FMT_NONE;
}

bool Decoder::DecodeAudioPacket(const AVStream* stream, AVCodecContext* ctx,
                                const AVPacket* pkt,
                                std::vector<AVFrameWrapper>* samples) {
  auto callback = [stream, samples](const AVFrame* av_frame) -> bool {
    // enum AVSampleFormat 定义参见 FFmpeg/libavutil/samplefmt.h
    std::vector<uint8_t> sample_data;

    // 将 PCM 数据从 av_frame 复制至 sample_data
    // sample_format = ExtractPCMData(sample_format, sample_number,
    // channel_number, av_frame, sample_data); if (sample_format ==
    // AV_SAMPLE_FMT_NONE) {
    //  LOG_ERROR << "extract pcm data failed";
    //  return false;
    //}

    samples->emplace_back(av_frame);
    AVFrameWrapper& sample = samples->back();
    sample->time_base = stream->time_base;

    // LOG_ERROR << "audio, pts in AVFrame: " << av_frame->pts << ", pts in
    // FrameParam: " << sample.param.pts
    //  << ", duration: " << sample.param.duration
    //  << ", stream->time_base: " << stream->time_base.num << "/" <<
    //  stream->time_base.den;

    return true;
  };
  return Packet2AVFrame(ctx, pkt, callback);
}

bool Decoder::Packet2AVFrame(AVCodecContext* ctx, const AVPacket* pkt,
                             const FrameDataExtractor& extractor) {
  // submit the packet to the decoder
  int ret = avcodec_send_packet(ctx, pkt);
  if (ret < 0) {
    LOG_ERROR << "error submitting a packet for decoding, " << av_err2str(ret);
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

}  // namespace util
}  // namespace live
