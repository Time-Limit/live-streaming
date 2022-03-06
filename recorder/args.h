#pragma once

#include <gflags/gflags.h>

namespace live {
namespace recorder {

DECLARE_string(input_video);
DECLARE_string(input_audio);
DECLARE_bool(enable_debug_renderer);
DECLARE_bool(list_devices);

}
}
