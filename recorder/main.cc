#include "util/util.h"
#include "util/env.h"

#include "recorder/args.h"
#include "recorder/base.h"
#include "recorder/context.h"

#include <gflags/gflags.h>

using namespace live::recorder;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_list_devices) {
    auto format_context_ = avformat_alloc_context();
    auto input_ = av_find_input_format("avfoundation");

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
    live::util::WaitSDLEventUntilCheckerReturnFalse([p = &c] () { return p->IsAlive(); });
  } catch (const std::string &err) {
    LOG_ERROR << err;
  }
  return 0;
}