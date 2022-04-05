#pragma once

#include "server/stream.h"
#include "util/util.h"

namespace live {
namespace util {
namespace rtmp {

struct ChunkHeader : public Protocol {
  struct {
    uint8_t format;
    uint32_t chunk_stream_id;
  } basic;

  struct Common {
    uint32_t timestamp;
    uint32_t length;
    uint8_t type;
    uint32_t message_stream_id;
  } common;

  uint32_t extended_timestamp;

  void Serialize(ByteStream& bs) const override;
  void Deserialize(ByteStream& bs) override;
};

// 在反序列网络字节流时, 这个类用来合并 ChunkMessage, 存储 Header 以及 Payload
struct Message : public Protocol {
  uint8_t type;
  uint32_t timestamp;
  uint32_t stream_id;
  uint32_t payload_length;

  std::vector<uint8_t> payload;

  void Serialize(ByteStream&) const override {
    throw std::runtime_error("not implemented Message::Serialize");
  }
  void Deserialize(ByteStream&) override {
    throw std::runtime_error("not implemented Message::Deserialize");
  }
};

// 将 Control, Command, Data 等 Message 序列化为 Chunk Stream
class RTMPSession;
class ChunkSerializeHelper : public Protocol {
  RTMPSession* session = nullptr;  // 用来获取各种参数，如 maxChunkSize 等
  Message&& message;

 public:
  ChunkSerializeHelper(RTMPSession* s, Message&& m)
      : session(s), message(std::move(m)) {}
  void Serialize(ByteStream&) const override;
  void Deserialize(ByteStream&) override {
    throw std::runtime_error(
        "not implemented ChunkSerializeHelper::Deserialize");
  }
};

}  // namespace rtmp
}  // namespace util
}  // namespace live
