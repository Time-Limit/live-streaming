#pragma once

#include "util/base.h"
#include "util/queue.h"
#include "util/util.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <future>
#include <unordered_map>

namespace live {
namespace util {

class Filter {
 public:
  struct OutputParam {
    int32_t width = 0;
    int32_t height = 0;
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    AVRational time_base = {0, 1};
  };

 private:
  using Source = std::tuple<const AVFilter*, AVFilterContext*>;
  using Sink = std::tuple<const AVFilter*, AVFilterContext*>;
  std::unordered_map<std::string, Source> sources_;
  std::unordered_map<std::string, Sink> sinks_;

  AVFilterInOut *source_inouts_ = nullptr, *sink_inouts_ = nullptr;
  AVFilterGraph* filter_graph_ = nullptr;

  Queue<std::pair<std::string, AVFrameWrapper>> input_queue_;
  Queue<AVFrameWrapper> output_queue_;
  AVFrameWrapper output_frame_;

  std::future<void> filter_future_;

  bool is_alive_ = true;

  using Callback = std::function<void(const AVFrameWrapper& frame)>;
  Callback callback_;

  OutputParam output_param_;

 public:
  Filter(const std::unordered_map<std::string, std::string>& source_param,
         const std::string& sink_name, const std::string& sink_param,
         const std::string& filter_description, Callback callback = Callback());
  ~Filter();

  bool Submit(const std::string& input, AVFrameWrapper&& frame);

  bool IsAlive() const {
    return is_alive_;
  }

  decltype(output_queue_)& GetOutputQueue();

  const OutputParam& GetOutputParam() const {
    return output_param_;
  }
};

}  // namespace util
}  // namespace live
