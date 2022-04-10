#include "server/net.h"
#include "server/rtmp.h"

#include <gflags/gflags.h>

using namespace live::util;

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  Listener(9527, &rtmp::RTMPSession::CreateRTMPSession).Listen();

  return 0;
}
