#pragma once

#include <gflags/gflags.h>

namespace live {
namespace recorder {

DECLARE_string(input_format);
DECLARE_string(input_video);
DECLARE_string(input_audio);
DECLARE_bool(enable_debug_renderer);
DECLARE_bool(enable_debug_speaker);
DECLARE_bool(enable_output_pts_info);
DECLARE_bool(list_devices);
DECLARE_string(url);

}  // namespace recorder
}  // namespace live
