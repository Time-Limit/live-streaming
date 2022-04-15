#pragma once

#include "server/chunk_message.h"
#include "server/command_message.h"
#include "server/control_message.h"
#include "server/handshake_message.h"
#include "server/net.h"
#include "server/stream.h"

#include "util/util.h"

namespace live {
namespace util {
namespace rtmp {

class RTMPSession : public Session {
  // 从客户端的视角定义的
  enum Type {
    UNDEFINED,
    PULL,
    PUSH,
  };

  Type type_ = Type::UNDEFINED;

  enum State {
    UNINTIALIZED,
    VERSION_SENT,
    ACK_SENT,
    HANDESHAKE_DONE,
  };

  State state_ = UNINTIALIZED;

  bool OnReadInUninitializedState();
  bool OnReadInVersionSentState();
  bool OnReadInAckSentState();
  bool OnReadInHandeShakeDoneState();

  bool SendS0AndS1();
  bool SendS2();

  HandshakeMessage0 c0;
  HandshakeMessage1 c1;
  HandshakeMessage2 c2;

  HandshakeMessage1 s1;
  HandshakeMessage2 s2;

  // 总是拼凑一个完整的 ChunkHeader::Common
  std::unordered_map<uint32_t, ChunkHeader::Common> previous_chunk_commons_;
  std::unordered_map<uint64_t, uint32_t> message_previous_timestamp_;

  // 正在从网络读取的 RTMP Message，
  // key 为 <chunk id, stream id>
  using MessageId = uint64_t;
  std::unordered_map<MessageId, Message> reading_messages_;
  uint32_t GetChunkIdFromMessageId(MessageId k) {
    return k >> 32;
  }
  uint32_t GetStreamIdFromMessageId(MessageId k) {
    return uint32_t(k);
  }
  MessageId GetMessageId(uint32_t csid, uint32_t msid) {
    return uint64_t(csid) << 32 | msid;
  }

  void HandleMessage(uint32_t csid, Message&& msg);
  void HandleCommandMessage(uint32_t csid, const Message& msg,
                            const CommandMessage& command);

  uint32_t max_chunk_size_ = 128;
  uint32_t max_chunk_size_for_sending_ = 128;

  uint32_t msid_for_create_stream_ = 16776960;

  int32_t room_id_ = -1;

 public:
  bool OnRead() override;
  void OnClose() override;

  void SendMetaData(const std::vector<uint8_t>& meta_payload);
  void SendMediaData(uint8_t type, uint32_t timestamp,
                     const std::vector<uint8_t>& payload);

  uint32_t GetChunkStreamIdForSending(const Message& msg) {
    switch (msg.type) {
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6: {
        return 2;
      }
      case 8:
      case 9:
      case 18: {
        return 4;
      }
      case 20: {
        return 3;
      }
    }
    LOG_ERROR << "not handle this type " << uint16_t(msg.type);
    assert(false);
    return -1;
  }

  uint32_t GetMaxChunkSizeForSending() {
    return max_chunk_size_for_sending_;
  }

 public:
  static std::unique_ptr<Session> CreateRTMPSession() {
    return std::unique_ptr<Session>(new RTMPSession());
  }
};

}  // namespace rtmp
}  // namespace util
}  // namespace live
