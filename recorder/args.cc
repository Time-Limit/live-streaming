#include "recorder/args.h"

namespace live {
namespace recorder {

DEFINE_string(input_format, "avfoundation",
              "The short name of the input format, it will be passed to "
              "av_find_input_format");

DEFINE_string(input_video, "1:1280x720:uyvy422:20,0:1280x720:uyvy422:20",
              "{url}:{WxH}:{pix_fmt}:{framerate} 至多有两个，用 ',' 分割");

DEFINE_string(input_audio, ":1", "{url} 至多有两个，用 ',' 分割");

DEFINE_bool(enable_debug_renderer, false, "是否开启调试用的 Renderer");
DEFINE_bool(enable_debug_speaker, false, "是否开启调试用的 Speaker");
DEFINE_bool(list_devices, false, "输出可用设备信息");
DEFINE_bool(enable_output_pts_info, false, "输出 PTS 信息");

DEFINE_string(url, "rtmp://127.0.0.1:9527", "url of rtmp server");

}  // namespace recorder
}  // namespace live
