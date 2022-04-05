#pragma once

#include "server/net.h"
#include "server/stream.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace live {
namespace util {
namespace rtmp {

class ControlMessage : public Message {};

struct AckWindowSize : public ControlMessage {
  AckWindowSize(uint32_t s = 0) : window_size(s) {
    Message::type = 5;
  }

  uint32_t window_size = 0;

  void Serialize(ByteStream& bs) const override {
    bs << window_size;
    LOG_ERROR << "window size " << window_size;
  }

  void Deserialize(ByteStream& bs) override {
    bs >> window_size;
    LOG_ERROR << "window_size: " << window_size;
  }
};

struct SetPeerBandwidth : public ControlMessage {
  SetPeerBandwidth(uint32_t s = 0, uint8_t l = 0) : window_size(s), limit(l) {
    Message::type = 6;
  }

  uint32_t window_size = 0;
  uint8_t limit = 0;

  void Serialize(ByteStream& bs) const override {
    bs << window_size << limit;
    LOG_ERROR << "window_size: " << window_size
              << ", limit: " << uint16_t(limit);
  }

  void Deserialize(ByteStream& bs) override {
    bs >> window_size >> limit;
    LOG_ERROR << "window_size: " << window_size
              << ", limit: " << uint16_t(limit);
  }
};

struct UserControlMessage : public Message {
  uint16_t event_type;
  UserControlMessage() {
    Message::type = 4;
  }
};

struct UserControlStreamBeginMessage : public UserControlMessage {
  UserControlStreamBeginMessage(uint32_t id) {
    event_type = 0;
    message_stream_id = id;
  }
  uint32_t message_stream_id = 0;

  void Serialize(ByteStream& bs) const override {
    bs << event_type << message_stream_id;
    LOG_ERROR << "event_type: " << event_type
              << ", message_stream_id: " << message_stream_id;
  }

  void Deserialize(ByteStream& bs) override {
    bs >> event_type >> message_stream_id;
    LOG_ERROR << "event_type: " << event_type
              << ", message_stream_id: " << message_stream_id;
  }
};

}  // namespace rtmp
}  // namespace util
}  // namespace live
