#include "util/util.h"
#include "util/env.h"

#include "recorder/args.h"
#include "recorder/base.h"
#include "recorder/context.h"

#include <gflags/gflags.h>

using namespace live::recorder;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  try {
    Context c;
    live::util::WaitSDLEventUntilCheckerReturnFalse([p = &c] () { return p->IsAlive(); });
  } catch (const std::string &err) {
    LOG_ERROR << err;
  }
  return 0;
}
