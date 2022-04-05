#include "stream.h"

namespace live {
namespace util {

void ByteStream::pop_bytes(uint8_t* ptr, size_t size, size_t len) {
  if (bytes_.size() - head_ < size) {
    throw ByteStream::NotEnoughException();
  }

  if (LocalHostIsLittleEndian()) {
    for (auto i = 0; i < size; i++) {
      ptr[size - i - 1] = bytes_[head_ + i];
    }
  } else {
    for (auto i = 0; i < size; i++) {
      ptr[i + len - size] = bytes_[head_ + i];
    }
  }
  head_ += size;
}

void ByteStream::push_bytes(const uint8_t* ptr, size_t size, size_t len) {
  if (LocalHostIsLittleEndian()) {
    for (auto i = 0; i < size; i++) {
      bytes_.emplace_back(ptr[size - i - 1]);
    }
  } else {
    for (auto i = 0; i < size; i++) {
      bytes_.emplace_back(ptr[i + len - size]);
    }
  }
}

ByteStream& ByteStream::operator>>(const Commit&) {
  if (head_ != 0) {
    memcpy(&bytes_[0], &bytes_[head_], bytes_.size() - head_);
  }
  bytes_.resize(bytes_.size() - head_);
  head_ = 0, tail_ = bytes_.size();
  return *this;
}

ByteStream& ByteStream::operator<<(const Commit& rhs) {
  return this->operator>>(rhs);
}

ByteStream& ByteStream::operator>>(const Revert&) {
  if (tail_ != bytes_.size()) {
    bytes_.resize(tail_);
  }
  head_ = 0, tail_ = bytes_.size();
  return *this;
}

ByteStream& ByteStream::operator<<(const Revert& rhs) {
  return this->operator>>(rhs);
}

ByteStream& ByteStream::operator>>(const Discard& d) {
  if (bytes_.size() - head_ < d.cnt) {
    head_ = bytes_.size();
  } else {
    head_ += d.cnt;
  }
  return *this;
}

ByteStream& ByteStream::operator<<(const Discard& rhs) {
  return this->operator>>(rhs);
}

ByteStream& ByteStream::operator>>(RawPtrWrapper&& wrapper) {
  if (wrapper.size > bytes_.size() - head_) {
    throw NotEnoughException();
  }
  memcpy(wrapper.ptr, &bytes_[head_], wrapper.size);
  head_ += wrapper.size;
  return *this;
}

ByteStream& ByteStream::operator>>(IntegerSizeWrapper&& rhs) {
  pop_bytes(rhs.ptr, rhs.size, rhs.len);
  return *this;
}

ByteStream& ByteStream::operator<<(const ConstRawPtrWrapper& rhs) {
  bytes_.insert(bytes_.end(), rhs.ptr, rhs.ptr + rhs.size);
  return *this;
}

ByteStream& ByteStream::operator<<(const ConstIntegerSizeWrapper& rhs) {
  push_bytes(rhs.ptr, rhs.size, rhs.len);
  return *this;
}

}  // namespace util
}  // namespace live
