#include "util/util.h"

#include <fstream>

namespace live {
namespace util {

bool ReadFile(const std::string& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::in | std::ios::binary);  //二进制读方式打开
  if (!file) {
    LOG_ERROR << "open failed, " << path << std::endl;
    return false;
  }
  file.seekg(0, file.end);
  auto size = file.tellg();
  file.seekg(0, file.beg);
  data.resize(size);
  file.read(reinterpret_cast<char*>(&data[0]), size);
  if (!file) {
    LOG_ERROR << "read failed, only " << file.gcount() << " could be read";
    file.close();
    return false;
  }
  file.close();
  return true;
}

}  // namespace util
}  // namespace live
