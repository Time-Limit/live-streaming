#include "player/args.h"

namespace live {
namespace player {

DEFINE_string(uri, "rtmp://127.0.0.1:9527?room=3", "尝试从该处获取媒体数据");
DEFINE_int32(window_width, 800, "窗口的宽度");
DEFINE_int32(window_height, 800, "窗口的高度");

}  // namespace player
}  // namespace live
