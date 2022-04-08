#include "server/rtmp.h"
#include "server/flv.h"

#include <fstream>

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
  } else if (command.name == "releaseStream" || command.name == "FCPublish" ||
             command.name == "FCUnpublish") {
    ByteStream(WriteDataBuffer())
        << ChunkSerializeHelper(this, CommandMessage("_result", command.id))
        << ByteStream::Commit();
  } else if (command.name == "createStream") {
    CommandMessage cm("_result", command.id);
    cm.obj1.marker = ActionScriptObject::Type::NULL_TYPE;
    cm.obj2.marker = ActionScriptObject::Type::DOUBLE;
    cm.obj2.double_value = GetPassedTimeSinceStartedInMicroSeconds();

    LOG_ERROR << "msg id for createStream command "
              << uint64_t(cm.obj2.double_value);

    ByteStream(WriteDataBuffer())
        << ChunkSerializeHelper(this, std::move(cm)) << ByteStream::Commit();
  } else if (command.name == "publish") {
    CommandMessage cm("onStatus", command.id);

    cm.obj1.marker = ActionScriptObject::Type::NULL_TYPE;

    std::shared_ptr<ActionScriptObject> desc(new ActionScriptObject());
    desc->marker = ActionScriptObject::Type::STRING;
    desc->string_value = "NetStream.Publish.Start";

    std::shared_ptr<ActionScriptObject> level(new ActionScriptObject());
    level->marker = ActionScriptObject::Type::STRING;
    level->string_value = "info";

    std::shared_ptr<ActionScriptObject> code(new ActionScriptObject());
    code->marker = ActionScriptObject::Type::STRING;
    code->string_value = "NetStream.Publish.Start";

    cm.obj2.marker = ActionScriptObject::Type::OBJECT;
    cm.obj2.dict_value["level"] = level;
    cm.obj2.dict_value["code"] = code;
    cm.obj2.dict_value["description"] = desc;

    ByteStream(WriteDataBuffer())
        << ChunkSerializeHelper(this, std::move(cm)) << ByteStream::Commit();
  } else if (command.name == "deleteStream") {
    // response nothing
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

std::once_flag write_file_header_flag;
static std::ofstream outfile("test.flv", std::ofstream::binary);
int cnt = 0;
int64_t start = 0;

void RTMPSession::HandleMessage(uint32_t csid, Message&& msg) {
  std::call_once(write_file_header_flag, []() {
    std::vector<uint8_t> header;
    ByteStream(header) << flv::Header(0x01 | 0x04) << uint32_t(0)
                       << ByteStream::Commit();

    outfile.write(reinterpret_cast<const char*>(&header[0]), header.size());

    LOG_ERROR << "header size " << header.size();

    start = GetPassedTimeSinceStartedInMicroSeconds();
  });

  try {
    switch (msg.type) {
      case 8:
      case 9: {
        LOG_ERROR << "msg.timestamp: " << msg.timestamp << ", msg.type: " << uint16_t(msg.type);

        flv::Tag tag(msg.type, msg.payload.size(), msg.timestamp);
        tag.data = msg.payload;

        std::vector<uint8_t> final_data;
        ByteStream(final_data)
            << tag << uint32_t(msg.payload.size()) << ByteStream::Commit();

        outfile.write(reinterpret_cast<const char*>(&final_data[0]),
                      final_data.size());
        outfile.flush();
        break;
      }
      case 18: {
        try {
          ByteStream bs(msg.payload);
          std::vector<ActionScriptObject> objs;
          while (bs.Remain()) {
            LOG_ERROR << "next stream, remain: " << bs.Remain();
            ActionScriptObject data;
            bs >> data;
            data.Output();
            if (data.marker != ActionScriptObject::Type::STRING ||
                data.string_value != "@setDataFrame") {
              objs.emplace_back(data);
            }
          }
          std::vector<uint8_t> final_data;
          for (auto& obj : objs) {
            ByteStream(final_data) << obj << ByteStream::Commit();
          }

          flv::Tag tag(flv::FLAG::SCRIPT_TAG, final_data.size(), msg.timestamp);
          std::swap(tag.data, final_data);

          ByteStream(final_data) << tag << ByteStream::Commit();

          outfile.write(reinterpret_cast<const char*>(&final_data[0]),
                        final_data.size());
          uint32_t size = final_data.size();
          std::vector<uint8_t> bytes;
          ByteStream(bytes) << size << ByteStream::Commit();
          outfile.write(reinterpret_cast<const char*>(&bytes[0]), 4);
          ByteStream(final_data).DumpBytes(1000);
          LOG_ERROR << "size: " << size;
          ByteStream(bytes).DumpBytes(1000);
        } catch (...) {
          LOG_ERROR << "parse ActionScriptObject failed";
        }
        break;
      }
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
  } catch (...) {
    LOG_ERROR << "got an exception";
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
    // LOG_ERROR << "data is not enough";
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
    // LOG_ERROR << "data is not enough";
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
    // LOG_ERROR << "data is not enough";
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
    // 先进性各种检查，在检查通过前，不能进行写入
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

    MessageId msg_id = GetMessageId(chunk_header.basic.chunk_stream_id,
                                    chunk_header.common.message_stream_id);

    uint32_t message_timestamp = 0;
    if (chunk_header.basic.format == 0) {
      message_timestamp = chunk_header.common.timestamp;
    } else {
      auto it = message_previous_timestamp_.find(msg_id);
      if (it == message_previous_timestamp_.end()) {
        LOG_ERROR << "not found message previous timestamp, msg_id: " << msg_id;
        return false;
      }
      message_timestamp = it->second + chunk_header.common.timestamp;
    }

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

    // 后续开始写入逻辑

    Message* msg = nullptr;
    if (it == reading_messages_.end()) {
      msg = &(reading_messages_[msg_id] = Message());

      msg->type = chunk_header.common.type;
      msg->timestamp = message_timestamp;
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

    //LOG_ERROR << "chunk received, csid: "
    //  << chunk_header.basic.chunk_stream_id
    //  << ", msid: " << chunk_header.common.message_stream_id
    //  << ", delta: " << chunk_header.common.timestamp
    //  << ", format: " << uint16_t(chunk_header.basic.format)
    //  << ", timestamp: " << message_timestamp
    //  << ", msg-type: " << uint16_t(chunk_header.common.type);

    bs >> ByteStream::Commit();

    if (chunk_header.basic.format == 0) {
      message_previous_timestamp_[msg_id] = message_timestamp;
    }

    if (msg->payload.size() == msg->payload_length) {
      LOG_ERROR << "message received, csid: "
                << chunk_header.basic.chunk_stream_id
                << ", msid: " << msg->stream_id
                << ", type: " << uint32_t(msg->type)
                << ", payload size: " << msg->payload_length;

      HandleMessage(chunk_header.basic.chunk_stream_id, std::move(*msg));

      reading_messages_.erase(msg_id);
      message_previous_timestamp_[msg_id] = message_timestamp;
    }

  } catch (const ByteStream::NotEnoughException& e) {
    // LOG_ERROR << "data is not enough";
    return true;
  }

  return true;
}

bool RTMPSession::OnRead() {
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
