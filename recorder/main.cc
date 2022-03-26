#include "util/util.h"
#include "util/env.h"

#include "recorder/args.h"
#include "recorder/base.h"
#include "recorder/context.h"

#include <gflags/gflags.h>
#include <csignal>

using namespace live::recorder;

bool is_alive = true;
void signal_handler(int signal) {
  is_alive = false;
}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_list_devices) {
    auto format_context_ = avformat_alloc_context();
    auto input_ = av_find_input_format(FLAGS_input_format.c_str());

    if (!input_) {
      LOG_ERROR << "not found input_format, " << FLAGS_input_format;
      return 0;
    }

    AVDictionary *dict = nullptr;
    av_dict_set(&dict, "list_devices", "1", 0);
    int ret = avformat_open_input(&format_context_, nullptr, input_, &dict);
    if (ret != -5 && ret < 0) {
      LOG_ERROR << "list device detail failed, ret: " << ret << ", " << av_err2str(ret);
    }
    av_dict_free(&dict);
    return 0;
  }
  try {
    Context c;
    std::signal(SIGINT, signal_handler);
    live::util::WaitSDLEventUntilCheckerReturnFalse([p = &c] () { return is_alive && p->IsAlive(); });
  } catch (const std::string &err) {
    LOG_ERROR << err;
  }
  return 0;
}
