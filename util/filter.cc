#include "util/filter.h"

namespace live {
namespace util {

Filter::Filter(const std::unordered_map<std::string, std::string> &source_param,
    const std::unordered_map<std::string, std::string> &sink_param,
    const std::string &filter_description) {

  filter_graph_ = avfilter_graph_alloc();

  if (!filter_graph_) {
    throw std::string("alloc filter graph failed");
  }

#define InitFilter(av_filter_name, filter_param, inout_link, container) \
  { \
    AVFilterInOut **head = &inout_link; \
    for (const auto &p : filter_param) { \
      const std::string &name = p.first; \
      const std::string &desc = p.second; \
      auto buffer = avfilter_get_by_name(av_filter_name);  \
      if (!buffer) { \
        throw std::string("not found filter, ") + av_filter_name; \
      } \
      AVFilterContext *buffer_context = nullptr; \
      auto ret = avfilter_graph_create_filter(&buffer_context, buffer, \
          name.c_str(), desc.c_str(), nullptr, filter_graph_); \
      if (ret < 0) {  \
        throw std::string("create filter failed, ") + av_err2str(ret) \
        + ", name: " + name + ", desc: " + desc; \
      } \
      *head = avfilter_inout_alloc(); \
      if (!head) { \
        throw std::string("create AVFilterInout failed"); \
      } \
      if (!container.insert(std::make_pair(name, std::make_tuple(buffer, buffer_context))).second) { \
        throw std::string("insert buffer failed, name: ") + name; \
      } \
      (*head)->name = av_strdup(name.c_str()); \
      (*head)->filter_ctx = buffer_context; \
      (*head)->pad_idx = 0; \
      (*head)->next = nullptr; \
      head = &((*head)->next); \
    } \
  }

  InitFilter("buffer", source_param, source_inouts_, sources_);
  InitFilter("buffersink", sink_param, sink_inouts_, sinks_);

#undef InitFilter

  auto ret = avfilter_graph_parse_ptr(filter_graph_,
      filter_description.c_str(),
      &source_inouts_, &sink_inouts_, nullptr);

  if (ret < 0) {
    throw std::string("parse filter description failed, ") + av_err2str(ret)
      + ", desc: " + filter_description;
  }
}

Filter::~Filter() {
  avfilter_graph_free(&filter_graph_);
  avfilter_inout_free(&source_inouts_);
  avfilter_inout_free(&sink_inouts_);
}

bool Filter::Submit(const std::string &input, Frame &&frame) {
  auto it = sources_.find(input);
  if (it == sources_.end()) {
    LOG_ERROR << "not found input buffer, " << input;
    return false;
  }
  AVFilterContext *context = std::get<2>(it->second);
  if (av_buffersrc_add_frame_flags(context, frame, AV_BUFFERSRC_FLAG_KEEP_REF)) {
  }
}

}
}
