#pragma once

#include "util/util.h"

namespace live {
namespace util {

class ByteStream;

struct Protocol {
  virtual void Serialize(ByteStream&) const = 0;
  virtual void Deserialize(ByteStream&) = 0;
};

class ByteStream {
  std::vector<uint8_t>& bytes_;
  size_t head_ = 0, tail_ = 0;

  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value, void>::type>
  void pop_bytes(T& v, size_t size = sizeof(T), size_t len = sizeof(T)) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&v);
    pop_bytes(ptr, size, len);
  }
  /*
   * @Param ptr，len: ptr 指向一段长度为 len 字节的内存
   * @Param size: 填充其中的低 size 个字节。
   * @Note 比如整个区间是 [0 ,len)，
   * 大端依次填充 ptr[len-size], ptr[len-size+1] ...
   * 小端依次填充 ptr[size-1], ptr[size-2] ...
   */
  void pop_bytes(uint8_t* ptr, size_t size, size_t len);

  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value, void>::type>
  void push_bytes(const T& v, size_t size = sizeof(T), size_t len = sizeof(T)) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&v);
    push_bytes(ptr, size, len);
  }
  void push_bytes(const uint8_t* ptr, size_t size, size_t len);

 public:
  ByteStream(std::vector<uint8_t>& bytes)
      : bytes_(bytes), head_(0), tail_(bytes_.size()) {}
  ~ByteStream() {
    this->operator>>(Revert());
  }

  size_t Remain() {
    return bytes_.size() - head_;
  }

  struct NotEnoughException {};

  struct RawPtrWrapper {
    RawPtrWrapper(uint8_t* p, size_t s) : ptr(p), size(s) {}
    uint8_t* ptr;
    size_t size;
  };

  struct ConstRawPtrWrapper {
    ConstRawPtrWrapper(const uint8_t* p, size_t s) : ptr(p), size(s) {}
    const uint8_t* ptr;
    size_t size;
  };

  struct IntegerSizeWrapper {
    template <
        typename T,
        typename = typename std::enable_if<
            std::is_integral<T>::value &&
                std::is_same<T, typename std::remove_const<T>::type>::value,
            void>::type>
    IntegerSizeWrapper(T& u, size_t s)
        : ptr(reinterpret_cast<uint8_t*>(&u)), size(s), len(sizeof(T)) {}
    uint8_t* ptr;
    size_t size;
    size_t len;
  };

  struct ConstIntegerSizeWrapper {
    template <typename T, typename = typename std::enable_if<
                              std::is_integral<T>::value, void>::type>
    ConstIntegerSizeWrapper(const T& u, size_t s)
        : ptr(reinterpret_cast<const uint8_t*>(&u)), size(s), len(sizeof(T)) {}
    const uint8_t* ptr;
    size_t size;
    size_t len;
  };

  struct Commit {};
  struct Revert {};
  struct Discard {
    Discard(size_t c) : cnt(c) {}
    size_t cnt;
  };

  ByteStream& operator>>(const Commit&);
  ByteStream& operator>>(const Revert&);
  ByteStream& operator>>(const Discard&);

  ByteStream& operator<<(const Commit&);
  ByteStream& operator<<(const Revert&);
  ByteStream& operator<<(const Discard&);

  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value, void>::type>
  ByteStream& operator>>(T& v) {
    pop_bytes(v);
    return *this;
  }
  ByteStream& operator>>(IntegerSizeWrapper&&);
  ByteStream& operator>>(RawPtrWrapper&&);
  ByteStream& operator>>(Protocol& p) {
    p.Deserialize(*this);
    return *this;
  }
  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value, void>::type>
  T Pop() {
    T v;
    this->operator>>(v);
    return v;
  }

  std::vector<uint8_t> PopUint8Vec(size_t len) {
    std::vector<uint8_t> data(len);
    this->operator>>(RawPtrWrapper(&data[0], len));
    return data;
  }

  std::string PopString(size_t len) {
    std::string data(len, 0);
    this->operator>>(RawPtrWrapper(reinterpret_cast<uint8_t*>(&data[0]), len));
    return data;
  }

  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value, void>::type>
  ByteStream& operator<<(T v) {
    push_bytes(v);
    return *this;
  }
  ByteStream& operator<<(const ConstRawPtrWrapper&);
  ByteStream& operator<<(const ConstIntegerSizeWrapper&);
  ByteStream& operator<<(const Protocol& p) {
    p.Serialize(*this);
    return *this;
  }

  void DumpBytes(size_t size) {
    for (int i = head_; i < bytes_.size() && i < head_ + size; i++) {
      LOG_ERROR << "i: " << i << ", v: " << bytes_[i] << ", "
                << uint32_t(bytes_[i]);
    }
  }
};

}  // namespace util
}  // namespace live
