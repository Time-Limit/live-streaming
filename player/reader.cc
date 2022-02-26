#include "player/reader.h"
#include "util/util.h"

extern "C" {
#include <libavutil/error.h>
#include <libavformat/avio.h>
}

namespace live {
namespace player {

LocalFileReader::LocalFileReader(const std::string &p)
    : Reader(), path_(p), file_(path_, std::ios::in|std::ios::binary), file_size_(-1) {
  if (!file_) {
    throw std::string("open file failed, path: ") + path_ + std::string(", reason: ") + strerror(errno);
  }
  file_.seekg(0, file_.end);
  file_size_ = file_.tellg();
  file_.seekg(0, file_.beg);
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

int64_t LocalFileReader::Seek(int64_t offset, int whence) {
  if (file_.eof()) {
    file_.clear(file_.eofbit);
  }
  if (file_.fail()) {
    file_.clear(file_.failbit);
  }
  switch (whence) {
    case AVSEEK_SIZE: {
      return file_size_;
    }
    case SEEK_SET: {
      file_.seekg(offset, file_.beg);
      return file_.tellg();
    }
    case SEEK_CUR: {
      file_.seekg(offset, file_.cur);
      return file_.tellg();
    }
    case SEEK_END: {
      file_.seekg(offset, file_.end);
      return file_.tellg();
    }
  }
  return -1;
}

}
}
