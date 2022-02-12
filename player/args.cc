#include "player/args.h"

namespace live {
namespace player {

DEFINE_string(local_file, "/path/to/mediafile", "若此值非空，则从本地加载其指向的文件");
DEFINE_string(url, "", "若 local_file 为空，则尝试从该链接处获取媒体数据");
DEFINE_string(format, "", "输入数据的封装格式，一般为文件的后缀名，如 flv，mp4 等");

}
}
