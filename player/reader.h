#pragma once

#include <string>
#include <fstream>
#include <cstdint>

namespace live {
namespace player {

class Reader {
 public:
 /*
  * @param buf 读出数据的缓冲区
  * @param size 缓冲区的大小
  * @return 成功则返回写入缓冲区的字节数，否则返回一个 AVERROR 对应的负数
  */
  virtual int Read(uint8_t *buf, int size) = 0;
  virtual int64_t Seek(int64_t offset, int whence) = 0;

  virtual ~Reader() = default;
};

class LocalFileReader : public Reader {
  std::string path_;
  std::ifstream file_; // 二进制读方式打开
  int64_t file_size_;
 public:
 /*
  * @param p 本地文件的路径
  * @note 如果加载失败，则会抛出一个 std::string 类型的异常，用于描述错误
  */
  LocalFileReader(const std::string &p);

  int Read(uint8_t *buf, int size) override;
  int64_t Seek(int64_t offset, int whence) override;
};

}
}
