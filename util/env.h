#pragma once

#include <thread>

namespace live {
namespace util {

extern const std::thread::id main_thread_id;

inline bool ThisThreadIsMainThread() {
  return std::this_thread::get_id() == main_thread_id;
}

void WaitSDLEventUntilCheckerReturnFalse(std::function<bool()> checker);

}  // namespace util
}  // namespace live
