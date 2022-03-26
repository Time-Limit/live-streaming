#include "util/filter.h"

namespace live {
namespace util {

Filter::Filter(const std::unordered_map<std::string, std::string> &source_param,
    const std::string &sink_name, const std::string &sink_param,
    const std::string &filter_description, Callback callback) {

  callback_ = std::move(callback);

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
      LOG_ERROR << "filter_type: " << av_filter_name << ", name: " << name << ", desc: " << desc; \
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
  InitFilter("buffersink", (std::unordered_map<std::string, std::string>{{sink_name, sink_param}}), sink_inouts_, sinks_);

#undef InitFilter

  auto ret = avfilter_graph_parse_ptr(filter_graph_,
      filter_description.c_str(),
      &sink_inouts_, &source_inouts_, nullptr);

  if (ret < 0) {
    throw std::string("parse filter description failed, ") + av_err2str(ret)
      + ", desc: " + filter_description;
  }

  if ((ret = avfilter_graph_config(filter_graph_, nullptr)) < 0) {
    throw std::string("avfilter_graph_config failed, ") + av_err2str(ret) + ", desc: " + filter_description;
  }

  output_frame_ = av_frame_alloc();
  if (!output_frame_.GetRawPtr()) {
    throw std::string("alloc output frame failed");
  }

  filter_future_ = std::async(std::launch::async, [this] () -> void {
    std::pair<std::string, AVFrameWrapper> data;
    while (is_alive_) {
      if (!input_queue_.TimedGet(&data, std::chrono::milliseconds(100))) {
        continue;
      }
      if (data.first.empty() || !data.second.GetRawPtr()) {
        continue;
      }
      auto &input = data.first;
      auto &frame = data.second;
      auto it = sources_.find(input);
      if (it == sources_.end()) {
        LOG_ERROR << "not found input buffer, " << input;
        continue;
      }
      AVFilterContext *context = std::get<1>(it->second);
      int ret = av_buffersrc_add_frame_flags(context, frame.GetRawPtr(), 0);
      if (ret < 0) {
        LOG_ERROR << "add frame into filter failed, input: " << input << ", error: " << av_err2str(ret);
        break;
      }
      while (true) {
        av_frame_unref(output_frame_.GetRawPtr());
        ret = av_buffersink_get_frame(std::get<1>(sinks_.begin()->second), output_frame_.GetRawPtr());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        if (ret < 0) {
          is_alive_ = false;
          break;
        }
        if (callback_) {
          callback_(output_frame_);
        } else {
          output_queue_.Put(output_frame_);
        }
      }
    }
  });
}

Filter::~Filter() {
  is_alive_ = false;
  if (filter_future_.valid()) {
    filter_future_.wait();
  }
  avfilter_graph_free(&filter_graph_);
  avfilter_inout_free(&source_inouts_);
  avfilter_inout_free(&sink_inouts_);
}

bool Filter::Submit(const std::string &input, AVFrameWrapper &&frame) {
  if (!is_alive_) {
    LOG_ERROR << "already killed";
    return false;
  }
  if (input_queue_.Size() > 5) {
    return false;
  }
  std::pair<std::string, AVFrameWrapper> res = std::make_pair(input, std::move(frame));
  input_queue_.Put(std::move(res));
  return true;
}

}
}
