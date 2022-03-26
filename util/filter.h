#pragma once

#include "util/util.h"
#include "util/base.h"
#include "util/queue.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#include <unordered_map>
#include <future>

namespace live {
namespace util {

class Filter {
  using Source = std::tuple<const AVFilter *, AVFilterContext *>;
  using Sink = std::tuple<const AVFilter *, AVFilterContext *>;
  std::unordered_map<std::string, Source> sources_;
  std::unordered_map<std::string, Sink> sinks_;

  AVFilterInOut *source_inouts_ = nullptr, *sink_inouts_ = nullptr;
  AVFilterGraph *filter_graph_ = nullptr;

  Queue<std::pair<std::string, AVFrameWrapper>> input_queue_;
  Queue<AVFrameWrapper> output_queue_;
  AVFrameWrapper output_frame_;

  std::future<void> filter_future_;

  bool is_alive_ = true;

  using Callback = std::function<void(const AVFrameWrapper &frame)>;
  Callback callback_;

 public:
  Filter(const std::unordered_map<std::string, std::string> &source_param,
      const std::string &sink_name, const std::string &sink_param,
      const std::string &filter_description, Callback callback = Callback());
  ~Filter();

  bool Submit(const std::string &input, AVFrameWrapper &&frame);

  bool IsAlive() const {
    return is_alive_;
  }

  decltype(output_queue_)& GetOutputQueue();
};

}
}
