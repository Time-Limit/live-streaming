#include "util/util.h"

#include <fstream>

namespace live {
namespace util {

static auto PROCESS_START_POINT =
    std::chrono::system_clock::now().time_since_epoch();

uint64_t GetPassedTimeSinceStartedInMicroSeconds() {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(now).count() -
         std::chrono::duration_cast<std::chrono::microseconds>(
             PROCESS_START_POINT)
             .count();
}

bool LocalHostIsLittleEndian() {
  union Layout {
    uint32_t ui32;
    uint8_t ui8[4];
  };
  Layout layout;
  layout.ui32 = 0x01020304;
  return layout.ui8[0] == 0x04;
}

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
