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
    stream << "\033[2m"
    << c
    << std::put_time(std::localtime(&t), "%H:%M:%S") << ':'
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

}
}

static std::time_t t2 = std::time(nullptr);

#define LOG_ERROR live::util::StreamLog(std::cerr, 'E', __FILE__, __LINE__, __FUNCTION__).ToStream()

#define LOG_INFO live::util::StreamLog(std::cout, 'I', __FILE__, __LINE__, __FUNCTION__).ToStream()

