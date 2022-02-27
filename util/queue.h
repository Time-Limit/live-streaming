#pragma once

#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <chrono>

namespace live {
namespace util {

template<typename T>
class Queue {
  std::queue<T> q_;
  uint32_t capacity_ = 0;
  mutable std::mutex m_;
  mutable std::condition_variable get_cv_;
  mutable std::condition_variable put_cv_;

  size_t PutWithoutLock(T &&t) {
    q_.push(std::forward<T>(t));
    return q_.size();
  }
 public:

  Queue(uint32_t cap = 0) : capacity_(cap) {}
  Queue(Queue&&) = default;

  size_t Size() const {
    std::unique_lock<std::mutex> g(m_);
    return q_.size();
  }

  size_t Put(const std::vector<T> &ts) {
    size_t size = 0;
    {
      std::unique_lock<std::mutex> g(m_);
      if (capacity_ && ts.size() + q_.size() > capacity_) {
        put_cv_.wait(g, [this]{return q_.size() < capacity_;});
      }
      for (auto t : ts) {
        PutWithoutLock(std::move(t));
      }
      size = q_.size();
    }
    get_cv_.notify_one();
    return size;
  }

  size_t Put(std::vector<T> &&ts) {
    size_t size = 0;
    {
      std::unique_lock<std::mutex> g(m_);
      if (capacity_ && ts.size() + q_.size() > capacity_) {
        put_cv_.wait(g, [this]{return q_.size() < capacity_;});
      }
      for (auto &t : ts) {
        PutWithoutLock(std::move(t));
      }
      size = q_.size();
    }
    get_cv_.notify_one();
    return size;
  }

  size_t Put(T &&t) {
    size_t size = 0;
    {
      std::unique_lock<std::mutex> g(m_);
      if (capacity_ && q_.size() >= capacity_) {
        put_cv_.wait(g, [this]{return q_.size() < capacity_;});
      }
      PutWithoutLock(std::forward<T>(t));
      size = q_.size();
    }
    get_cv_.notify_one();
    return size;
  }

  bool TimedPut(T &&t, const std::chrono::milliseconds &timeout) {
    {
      std::unique_lock<std::mutex> g(m_);
      if (capacity_ && q_.size() >= capacity_) {
        put_cv_.wait_for(g, timeout, [this]{return q_.size() < capacity_;});
      }
      if (capacity_ && q_.size() >= capacity_) {
        return false;
      }
      PutWithoutLock(std::forward<T>(t));
    }
    get_cv_.notify_one();
    return true;
  }

  bool TryToGet(T *t) {
    bool need_notify_get = false;
    bool need_notify_put = false;
    {
      std::unique_lock<std::mutex> g(m_);
      if (q_.empty()) {
        return false;
      }
      *t = std::move(q_.front());
      q_.pop();
      if (q_.size()) {
        need_notify_get = true;
      }
      if (q_.size() < capacity_) {
        need_notify_put = true;
      }
    }
    if (need_notify_get) {
      get_cv_.notify_one();
    }
    if (need_notify_put) {
      put_cv_.notify_one();
    }
    return true;
  }

  bool TimedGet(T *t, const std::chrono::milliseconds &timeout) {
    bool need_notify_get = false;
    bool need_notify_put = false;
    {
      std::unique_lock<std::mutex> g(m_);
      if (q_.empty()) {
        get_cv_.wait_for(g, timeout, [this]{return q_.size();});
      }
      if (q_.empty()) {
        return false;
      }
      *t = std::move(q_.front());
      q_.pop();
      if (q_.size()) { need_notify_get = true; }
      if (q_.size() < capacity_) { need_notify_put = true; }
    }
    if (need_notify_get) { get_cv_.notify_one(); }
    if (need_notify_put) { put_cv_.notify_one(); }
    return true;
  }

  bool Get(T *t) {
    bool need_notify_get = false;
    bool need_notify_put = false;
    {
      std::unique_lock<std::mutex> g(m_);
      if (q_.empty()) {
        get_cv_.wait(g, [this]{return q_.size();});
      }
      if (q_.empty()) {
        return false;
      }
      *t = std::move(q_.front());
      q_.pop();
      if (q_.size()) { need_notify_get = true; }
      if (q_.size() < capacity_) { need_notify_put = true; }
    }
    if (need_notify_get) { get_cv_.notify_one(); }
    if (need_notify_put) { put_cv_.notify_one(); }
    return true;
  }

  void Clear() {
    std::unique_lock<std::mutex> g(m_);
    q_ = std::queue<T>();
  }
};

}
}
