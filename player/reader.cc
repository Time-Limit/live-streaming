#include "player/reader.h"

extern "C" {
#include <libavutil/error.h>
}

namespace live {
namespace player {

LocalFileReader::LocalFileReader(const std::string &p)
    : Reader(), path_(p), file_(path_, std::ios::in|std::ios::binary) {
  if (!file_) {
    throw std::string("open file failed, path: ") + path_ + std::string(", reason: ") + strerror(errno);
  }
}

int LocalFileReader::Read(uint8_t *buf, int size) {
  if (size <= 0) {
    return 0;
  }
  file_.read(reinterpret_cast<char *>(buf), size);
  int ret = file_.gcount();
  if (ret == 0) {
    return AVERROR_EOF;
  }
  return ret;
}

}
}
