#include "server/net.h"
#include "server/rtmp.h"

#include <gflags/gflags.h>

using namespace live::util;

DEFINE_int32(port, 9527, "对外提供服务的端口");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  Listener(FLAGS_port, &rtmp::RTMPSession::CreateRTMPSession).Listen();

  return 0;
}
