#pragma once

#include "server/command_message.h"
#include "server/net.h"
#include "server/stream.h"

#include "util/util.h"

namespace live {
namespace util {
namespace rtmp {

struct HandshakeMessage0 : public Protocol {
  uint8_t version;
  void Serialize(ByteStream& bs) const override {
    bs << version;
  }
  void Deserialize(ByteStream& bs) override {
    bs >> version;
  }
};

struct CommonHandshakeMessage : public Protocol {
  uint32_t timestamp = 0;
  uint32_t timestamp_sent = 0;
  uint8_t random_data[1528];

  void Serialize(ByteStream& bs) const override {
    bs << timestamp << timestamp_sent
       << ByteStream::ConstRawPtrWrapper(&random_data[0], sizeof(random_data));
  }
  void Deserialize(ByteStream& bs) override {
    bs >> timestamp >> timestamp_sent >>
        ByteStream::RawPtrWrapper(&random_data[0], sizeof(random_data));
  }
};
using HandshakeMessage1 = CommonHandshakeMessage;
using HandshakeMessage2 = CommonHandshakeMessage;

}  // namespace rtmp
}  // namespace util
}  // namespace live
