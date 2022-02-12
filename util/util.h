#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <bitset>

namespace live {
namespace util {

bool ReadFile(const std::string &path, std::vector<uint8_t> &data);

struct StreamLog {
  std::ostream &stream;
  StreamLog(std::ostream &s, const char *file, int line, const char *func) : stream(s) {
    stream << "\033[2m"
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

#define LOG_ERROR live::util::StreamLog(std::cerr, __FILE__, __LINE__, __FUNCTION__).ToStream()

#define LOG_INFO live::util::StreamLog(std::cout, __FILE__, __LINE__, __FUNCTION__).ToStream()

