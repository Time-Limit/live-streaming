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

    //LOG_ERROR << "video, pts in AVFrame: " << frame->pts
    //          << ", stream->time_base: " << stream->time_base.num << "/"
    //          << stream->time_base.den;

    return true;
  };
  return Packet2AVFrame(ctx, pkt, callback);
}

bool Decoder::DecodeAudioPacket(const AVStream* stream, AVCodecContext* ctx,
                                const AVPacket* pkt,
                                std::vector<AVFrameWrapper>* samples) {
  auto callback = [stream, samples](const AVFrame* av_frame) -> bool {
    // enum AVSampleFormat 定义参见 FFmpeg/libavutil/samplefmt.h
    std::vector<uint8_t> sample_data;

    samples->emplace_back(av_frame);
    AVFrameWrapper& sample = samples->back();
    sample->time_base = stream->time_base;

    // LOG_ERROR << "audio, pts in AVFrame: " << av_frame->pts
    //           << ", stream->time_base: " << stream->time_base.num << "/"
    //           << stream->time_base.den
    //           << ", best_effort_timestamp: " << av_frame->best_effort_timestamp
    //           << ", pts in pkt: " << pkt->pts << ", dts in pkt: " << pkt->dts
    //           << ", sample_number: " << av_frame->nb_samples
    //           << ", sampel_rate: " << av_frame->sample_rate;

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
