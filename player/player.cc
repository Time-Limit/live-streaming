#include "player/args.h"
#include "player/context.h"
#include "util/util.h"

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  live::player::Context context;

  if (live::player::FLAGS_local_file.empty()) {
    LOG_ERROR << "Usage: ./player-client --local_file /path/to/file";
    return -1;
  }

  if (!context.InitLocalFileReader(live::player::FLAGS_local_file)) {
    return -1;
  }

  if (!context.InitDecoder(live::player::FLAGS_format)) {
    return -1;
  }

  return 0;
}
