#pragma once

#include "util/util.h"

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
}

#include <unordered_map>

namespace live {
namespace util {

class Frame;

class Filter {
  using Source = std::tuple<const AVFilter *, AVFilterContext *>;
  using Sink = std::tuple<const AVFilter *, AVFilterContext *>;
  std::unordered_map<std::string, Source> sources_;
  std::unordered_map<std::string, Sink> sinks_;

  AVFilterInOut *source_inouts_ = nullptr, *sink_inouts_ = nullptr;
  AVFilterGraph *filter_graph_ = nullptr;
 public:
  Filter(const std::unordered_map<std::string, std::string> &source_param,
      const std::unordered_map<std::string, std::string> &sink_param,
      const std::string &filter_description);
  ~Filter();

  bool Submit(const std::string &input, Frame &&frame);
};

}
}
