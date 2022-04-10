#pragma once

#include "server/stream.h"

namespace live {
namespace util {
namespace flv {

enum FLAG {
  HAS_VIDEO = 0x01,
  HAS_AUDIO = 0x04,

  AUDIO_TAG = 8,
  VIDEO_TAG = 9,
  SCRIPT_TAG = 18,
};

struct Header : public Protocol {
  char signature[3] = {'F', 'L', 'V'};
  uint8_t version = 1;
  uint8_t flags;
  uint32_t data_offset = 9;

  Header(uint8_t f = HAS_VIDEO | HAS_AUDIO) : flags(f) {}

  void Serialize(ByteStream& bs) const override {
    bs << signature[0] << signature[1] << signature[2] << version << flags
       << data_offset;
  }

  void Deserialize(ByteStream& bs) override {
    bs >> signature[0] >> signature[1] >> signature[2] >> version >> flags >>
        data_offset;
  }
};

struct TagHeader : public Protocol {
  uint8_t type;
  uint32_t data_size;
  uint32_t timestamp;
  uint32_t stream_id = 0;

  TagHeader(uint8_t t, uint32_t d, uint32_t ti)
      : type(t), data_size(d), timestamp(ti) {}

  void Deserialize(ByteStream& bs) override {
    uint8_t extend;
    bs >> type >> ByteStream::IntegerSizeWrapper(data_size, 3) >>
        ByteStream::IntegerSizeWrapper(timestamp, 3) >> extend >>
        ByteStream::IntegerSizeWrapper(stream_id, 3);
    timestamp |= (uint32_t(extend) << 24);
  }

  void Serialize(ByteStream& bs) const override {
    bs << type << ByteStream::ConstIntegerSizeWrapper(data_size, 3)
       << ByteStream::ConstIntegerSizeWrapper(timestamp, 3)
       << uint8_t(timestamp >> 24)
       << ByteStream::ConstIntegerSizeWrapper(stream_id, 3);
  }
};

struct Tag : public Protocol {
  TagHeader header;
  std::vector<uint8_t> data;

  Tag(uint8_t t, uint32_t d, uint32_t ti) : header(t, d, ti) {}

  void Deserialize(ByteStream& bs) override {
    bs >> header;
    data.resize(header.data_size);
    bs >> ByteStream::RawPtrWrapper(&data[0], data.size());
  }

  void Serialize(ByteStream& bs) const override {
    bs << header << ByteStream::ConstRawPtrWrapper(&data[0], data.size());
  }
};

}  // namespace flv
}  // namespace util
}  // namespace live
