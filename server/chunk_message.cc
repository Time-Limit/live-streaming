#include "server/chunk_message.h"
#include "server/control_message.h"
#include "server/rtmp.h"

namespace live {
namespace util {
namespace rtmp {

void ChunkHeader::Serialize(ByteStream& bs) const {
  if (basic.chunk_stream_id <= 63) {
    uint8_t byte = uint8_t(basic.format << 6) | uint8_t(basic.chunk_stream_id);
    bs << byte;
  } else if (basic.chunk_stream_id <= 319) {
    uint8_t byte = uint8_t(basic.format << 6) & uint8_t(0xC0);
    bs << byte << uint8_t(basic.chunk_stream_id - 64);
  } else {
    uint8_t byte = (uint8_t(basic.format << 6) & uint8_t(0xC0)) | uint8_t(1);
    bs << byte << uint16_t(basic.chunk_stream_id - 64);
  }

  uint32_t extended_timestamp = 0;
  uint32_t timestamp = common.timestamp;
  if (common.timestamp > 0x00FFFFFF) {
    extended_timestamp = common.timestamp;
    timestamp = 0x00FFFFFF;
  }

  switch (basic.format) {
    case 0: {
      bs << ByteStream::ConstIntegerSizeWrapper(timestamp, 3);
      bs << ByteStream::ConstIntegerSizeWrapper(common.length, 3);
      bs << common.type;
      bs << common.message_stream_id;
      break;
    }
    case 1: {
      bs << ByteStream::ConstIntegerSizeWrapper(timestamp, 3);
      bs << ByteStream::ConstIntegerSizeWrapper(common.length, 3);
      bs << common.type;
      break;
    }
    case 2: {
      bs << ByteStream::ConstIntegerSizeWrapper(timestamp, 3);
      break;
    }
    case 3: {
      break;
    }
  }

  if (extended_timestamp) {
    bs << extended_timestamp;
  }
}

void ChunkHeader::Deserialize(ByteStream& bs) {
  bs >> basic.format;

  basic.chunk_stream_id = (basic.format & 0x3F);
  basic.format >>= 6;

  switch (basic.chunk_stream_id) {
    case 0: {
      uint8_t byte;
      bs >> byte;
      basic.chunk_stream_id = byte + 64;
      break;
    }
    case 1: {
      uint16_t two_bytes;
      bs >> two_bytes;
      basic.chunk_stream_id = two_bytes + 64;
      break;
    }
  }

  auto format = basic.format;

  switch (format) {
    case 0: {
      bs >> ByteStream::IntegerSizeWrapper(common.timestamp, 3);
      bs >> ByteStream::IntegerSizeWrapper(common.length, 3);
      bs >> common.type;
      bs >> common.message_stream_id;
      break;
    }
    case 1: {
      bs >> ByteStream::IntegerSizeWrapper(common.timestamp, 3);
      bs >> ByteStream::IntegerSizeWrapper(common.length, 3);
      bs >> common.type;
      break;
    }
    case 2: {
      bs >> ByteStream::IntegerSizeWrapper(common.timestamp, 3);
      break;
    }
    case 3: {
      break;
    }
  }

  if (common.timestamp == 0x00FFFFFF) {
    bs >> extended_timestamp;
  }
}

void ChunkSerializeHelper::Serialize(ByteStream& bs) const {
  std::vector<uint8_t> payload;
  ByteStream(payload) << message << ByteStream::Commit();

  if (payload.empty()) {
    LOG_ERROR << "empty payload, type: " << message.type;
    assert(false);
  }

  uint32_t limit = session->GetMaxChunkSizeForSending();

  ChunkHeader header;
  header.basic.format = 0;
  header.basic.chunk_stream_id = session->GetChunkStreamIdForSending(message);

  if (header.common.type >= 7) {
    header.common.timestamp = GetPassedTimeSinceStartedInMicroSeconds() / 1000;
    header.common.message_stream_id = GetPassedTimeSinceStartedInMicroSeconds();
  } else {
    header.common.timestamp = 0;
    header.common.message_stream_id = 0;
  }

  header.common.length = payload.size();
  header.common.type = message.type;

  for (uint32_t i = 0; i < payload.size(); i += limit) {
    uint32_t len = std::min(limit, uint32_t(payload.size() - i));
    bs << header << ByteStream::ConstRawPtrWrapper(&payload[i], len);
    header.basic.format = 3;
  }
}

}  // namespace rtmp
}  // namespace util
}  // namespace live
