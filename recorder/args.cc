#include "recorder/args.h"

namespace live {
namespace recorder {

DEFINE_string(input_video, "0:1280x720:uyvy422:30:10_20_1_128_72",
  "{url}:{WxH}:{pix_fmt}:{framerate}:{x_y_z_w_h} 至多有两个，用 ';' 分割");

DEFINE_string(input_audio, ":1",
  "{url}:{WxH}:{pix_fmt}:{framerate}:{x_y_z_w_h} 至多有两个，用 ';' 分割");

DEFINE_bool(enable_debug_renderer, false, "是否开启调试用的渲染器");
DEFINE_bool(list_devices, false, "输出可用设备信息");

}
}
