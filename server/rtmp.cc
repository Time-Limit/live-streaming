#include "server/rtmp.h"

namespace live {
namespace util {
namespace rtmp {

void RTMPSession::HandleCommandMessage(uint32_t csid, const Message& msg,
                                       const CommandMessage& command) {
  if (command.name == "connect") {
    ByteStream bs(WriteDataBuffer());
    bs << ChunkSerializeHelper(this, AckWindowSize(1024))
       << ChunkSerializeHelper(this, SetPeerBandwidth(1024, 1))
       << ChunkSerializeHelper(this,
                               UserControlStreamBeginMessage(
                                   GetPassedTimeSinceStartedInMicroSeconds()))
       << ChunkSerializeHelper(this, CommandMessage("_result", command.id))
       << ByteStream::Commit();
  } else if (command.name == "releaseStream" || command.name == "FCPublish") {
    ByteStream(WriteDataBuffer())
        << ChunkSerializeHelper(this, CommandMessage("_result", command.id))
        << ByteStream::Commit();
  } else if (command.name == "createStream") {
    CommandMessage cm("_result", command.id);
    cm.obj2.marker = ActionScriptObject::Type::DOUBLE;
    cm.obj2.double_value = GetPassedTimeSinceStartedInMicroSeconds();

    LOG_ERROR << "msg id for createStream command " << cm.obj2.double_value;

    ByteStream(WriteDataBuffer())
        << ChunkSerializeHelper(this, std::move(cm)) << ByteStream::Commit();
  } else {
    LOG_ERROR << "not handle this command " << command.name;
    assert(false);
    return;
  }

  if (!Write()) {
    LOG_ERROR << "send command response failed";
    return;
  }
}

void RTMPSession::HandleMessage(uint32_t csid, Message&& msg) {
  switch (msg.type) {
    case 20: {
      try {
        CommandMessage command;
        ByteStream(msg.payload) >> command;
        HandleCommandMessage(csid, msg, command);
      } catch (...) {
        LOG_ERROR << "invalid command";
        Session::SetFlag(Session::FLAG::NEED_CLOSE);
      }
      break;
    }
    default: {
      LOG_ERROR << "not handler this type " << uint32_t(msg.type);
    }
  }
}

bool RTMPSession::SendS0AndS1() {
  {
    HandshakeMessage0 s0;
    s0.version = 3;

    HandshakeMessage1 s1;
    s1.timestamp = GetPassedTimeSinceStartedInMicroSeconds() / 1000;
    s1.timestamp_sent = 0;

    ByteStream(WriteDataBuffer()) << s0 << s1 << ByteStream::Commit();

    LOG_ERROR << "s1.timestamp: " << s1.timestamp
              << ", write buffer size: " << WriteDataBuffer().size();
  }

  if (!Write()) {
    LOG_ERROR << "SendS0AndS1 failed";
    return false;
  }

  return true;
}

bool RTMPSession::SendS2() {
  {
    HandshakeMessage2 s2;
    s2.timestamp = GetPassedTimeSinceStartedInMicroSeconds() / 1000;
    s2.timestamp_sent = c1.timestamp;
    memcpy(s2.random_data, c1.random_data, sizeof(s2.random_data));

    ByteStream(WriteDataBuffer()) << s2 << ByteStream::Commit();

    LOG_ERROR << "s2.timestamp: " << s2.timestamp
              << ", s2.timestamp_sent: " << s2.timestamp_sent
              << ", write buffer size: " << WriteDataBuffer().size();
  }

  if (!Write()) {
    LOG_ERROR << "SendS2 failed";
    return false;
  }

  return true;
}

bool RTMPSession::OnReadInUninitializedState() {
  try {
    ByteStream(ReadDataBuffer()) >> c0 >> ByteStream::Commit();

    if (c0.version != 3) {
      LOG_ERROR << "only support version3, but got " << uint32_t(c0.version);
      return false;
    }
  } catch (const ByteStream::NotEnoughException& e) {
    LOG_ERROR << "data is not enough";
    return true;
  }

  if (!SendS0AndS1()) {
    return false;
  }

  state_ = VERSION_SENT;
  // 后续数据可能已经到了，尝试一下
  return OnReadInVersionSentState();
}

bool RTMPSession::OnReadInVersionSentState() {
  try {
    ByteStream(ReadDataBuffer()) >> c1 >> ByteStream::Commit();
  } catch (const ByteStream::NotEnoughException& e) {
    LOG_ERROR << "data is not enough";
    return true;
  }

  LOG_ERROR << "c1.timestamp: " << c1.timestamp
            << ", c1.timestamp_sent: " << c1.timestamp_sent;

  if (!SendS2()) {
    LOG_ERROR << "SendS2 failed";
    return false;
  }

  state_ = ACK_SENT;

  return OnReadInAckSentState();
}

bool RTMPSession::OnReadInAckSentState() {
  try {
    ByteStream(ReadDataBuffer()) >> c2 >> ByteStream::Commit();
  } catch (const ByteStream::NotEnoughException& e) {
    LOG_ERROR << "data is not enough";
    return true;
  }

  LOG_ERROR << "c2.timestamp: " << c2.timestamp
            << ", c2.timestamp_sent: " << c2.timestamp_sent;

  state_ = HANDESHAKE_DONE;

  return OnReadInHandeShakeDoneState();
}

bool RTMPSession::OnReadInHandeShakeDoneState() {
  ChunkHeader chunk_header;
  try {
    ByteStream bs(ReadDataBuffer());
    bs >> chunk_header;

    switch (chunk_header.basic.format) {
      case 0: {
        break;
      }
      case 1: {
        auto it =
            previous_chunk_commons_.find(chunk_header.basic.chunk_stream_id);
        if (it == previous_chunk_commons_.end()) {
          LOG_ERROR << "not found cached chunk in previous chunks for csid: "
                    << chunk_header.basic.chunk_stream_id;
          return false;
        }
        // Type-1 的字段好奇怪，复用了 message_stream_id 却未复用
        // message_type_id 和 length 难道一个 message 还能有多种 type 和 length
        // ?
        chunk_header.common.message_stream_id = it->second.message_stream_id;
        break;
      }
      case 2: {
        auto it =
            previous_chunk_commons_.find(chunk_header.basic.chunk_stream_id);
        if (it == previous_chunk_commons_.end()) {
          LOG_ERROR << "not found cached chunk in previous chunks for csid: "
                    << chunk_header.basic.chunk_stream_id;
          return false;
        }
        chunk_header.common.length = it->second.length;
        chunk_header.common.type = it->second.type;
        chunk_header.common.message_stream_id = it->second.message_stream_id;
        break;
      }
      case 3: {
        auto it =
            previous_chunk_commons_.find(chunk_header.basic.chunk_stream_id);
        if (it == previous_chunk_commons_.end()) {
          LOG_ERROR << "not found cached chunk in previous chunks for csid: "
                    << chunk_header.basic.chunk_stream_id;
          return false;
        }
        chunk_header.common = it->second;
        break;
      }
    }

    LOG_ERROR << "format is " << uint32_t(chunk_header.basic.format)
              << ", csid is " << chunk_header.basic.chunk_stream_id;

    LOG_ERROR << "type: " << uint32_t(chunk_header.common.type)
              << ", length: " << chunk_header.common.length
              << ", timestamp: " << chunk_header.common.timestamp
              << ", message_stream_id: "
              << chunk_header.common.message_stream_id
              << ", extended_timestamp: " << chunk_header.extended_timestamp;

    MessageId msg_id = GetMessageId(chunk_header.basic.chunk_stream_id,
                                    chunk_header.common.message_stream_id);

    auto it = reading_messages_.find(msg_id);
    auto expect_data_len = max_chunk_size_;
    if (it == reading_messages_.end()) {
      expect_data_len = std::min(expect_data_len, chunk_header.common.length);
    } else {
      expect_data_len = std::min(
          expect_data_len,
          uint32_t(it->second.payload_length - it->second.payload.size()));
    }

    if (bs.Remain() < expect_data_len) {
      throw ByteStream::NotEnoughException();
    }

    Message* msg = nullptr;
    if (it == reading_messages_.end()) {
      if (chunk_header.basic.format != 0) {
        LOG_ERROR << "expect type-0 chunk, but got "
                  << uint16_t(chunk_header.basic.format)
                  << ", csid: " << chunk_header.basic.chunk_stream_id
                  << ", msid: " << chunk_header.common.message_stream_id;
      }

      msg = &(reading_messages_[msg_id] = Message());

      msg->type = chunk_header.common.type;
      msg->timestamp = chunk_header.common.timestamp;
      msg->stream_id = chunk_header.common.message_stream_id;
      msg->payload_length = chunk_header.common.length;

    } else {
      msg = &(it->second);
    }

    msg->payload.resize(msg->payload.size() + expect_data_len);
    bs >> ByteStream::RawPtrWrapper(
              &msg->payload[msg->payload.size() - expect_data_len],
              expect_data_len);

    previous_chunk_commons_[chunk_header.basic.chunk_stream_id] =
        chunk_header.common;

    bs >> ByteStream::Commit();

    if (msg->payload.size() == msg->payload_length) {
      LOG_ERROR << "message is received, csid: "
                << chunk_header.basic.chunk_stream_id
                << ", msid: " << msg->stream_id
                << ", type: " << uint32_t(msg->type)
                << ", payload size: " << msg->payload_length;

      HandleMessage(chunk_header.basic.chunk_stream_id, std::move(*msg));

      reading_messages_.erase(msg_id);
    }

  } catch (const ByteStream::NotEnoughException& e) {
    LOG_ERROR << "data is not enough";
    return true;
  }

  return true;
}

bool RTMPSession::OnRead() {
  LOG_ERROR << "current state_ is " << state_;
  switch (state_) {
    case UNINTIALIZED: {
      return OnReadInUninitializedState();
    }
    case VERSION_SENT: {
      return OnReadInVersionSentState();
    }
    case ACK_SENT: {
      return OnReadInAckSentState();
    }
    case HANDESHAKE_DONE: {
      return OnReadInHandeShakeDoneState();
    }
    default: {
      LOG_ERROR << "not handle this state " << state_;
      return false;
    }
  }
}

}  // namespace rtmp
}  // namespace util
}  // namespace live
