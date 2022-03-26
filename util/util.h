#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <ctime>

namespace live {
namespace util {

bool ReadFile(const std::string &path, std::vector<uint8_t> &data);

struct StreamLog {
  std::ostream &stream;
  StreamLog(std::ostream &s, char c, const char *file, int line, const char *func) : stream(s) {
    std::time_t t = std::time(nullptr);
    auto now = std::chrono::system_clock::now().time_since_epoch();
    char msecs[4] = {0};
    snprintf(msecs, 4, "%03lld", std::chrono::duration_cast<std::chrono::microseconds>(now).count()%1000);
    //auto msecs = std::chrono::duration_cast<std::chrono::microseconds>(now).count()%1000;
    stream << "\033[2m"
    << c
    << std::put_time(std::localtime(&t), "%H:%M:%S") << '.' << msecs << ':'
    << file << ':'
    << std::setfill('0') << std::setw(5) << line << ':'
    << func << ": " << "\033[0m";
  }
  ~StreamLog() {
    stream << std::endl;
  }
  std::ostream& ToStream() {
    return stream;
  }
};

template<typename EF>
class ScopeGuard {
  EF exit_function_;
 public:
  ScopeGuard(EF &&ef) : exit_function_(std::move(ef)) {}
  ~ScopeGuard() { exit_function_(); }
  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard& operator= (const ScopeGuard &) = delete;
};

}
}

static std::time_t t2 = std::time(nullptr);

#define LOG_ERROR live::util::StreamLog(std::cerr, 'E', __FILE__, __LINE__, __FUNCTION__).ToStream()

#define LOG_INFO live::util::StreamLog(std::cout, 'I', __FILE__, __LINE__, __FUNCTION__).ToStream()

