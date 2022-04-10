#pragma once

#include "server/rtmp.h"
#include "util/queue.h"

#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace live {
namespace util {

class Room {
  std::vector<uint8_t> meta_payload_;

  bool is_alive_;

  std::vector<uint8_t> aac_header_payload_;
  // 缓存最近一个关键帧及其之后的非关键帧
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> cached_video_payload_;

  struct State {
    bool has_sent_audio = false;
    bool has_sent_video = false;
  };

  std::unordered_map<rtmp::RTMPSession*, State> visitors_;

 public:
  Room() : is_alive_(true) {}

  ~Room() {
    is_alive_ = false;
  }

  void InitMetaData(const std::vector<uint8_t>& mp) {
    meta_payload_ = mp;
    // broadcast to all visitors
    for (auto v : visitors_) {
      v.first->SendMetaData(meta_payload_);
    }
  }

  void AddData(uint8_t type, uint32_t timestamp,
               const std::vector<uint8_t>& payload) {
    // 现在只能广播 AAC 和 H264
    // broadcast to all visitors

    if (payload.empty()) { return; }
    
    bool is_key_frame = false;
    if (type == 9) {
      is_key_frame = ((payload[0]>>4) == 1);
      if (is_key_frame) {
        cached_video_payload_.resize(0);
      }
      cached_video_payload_.emplace_back(std::make_pair(timestamp, payload));
    }

    bool is_aac_seq_header = false;
    if (type == 8) {
      // AAC 会有 1 byte 的情形吗？
      is_aac_seq_header = ((payload[0]>>4) == 10 && (payload[1] == 0));
      if (is_aac_seq_header) {
        aac_header_payload_ = payload;
      }
    }

    for (auto &v : visitors_) {
      // 8 音频，9 视频
      if (type == 8) {
        if (v.second.has_sent_audio || is_aac_seq_header) {
          v.first->SendMediaData(type, timestamp, payload);
          v.second.has_sent_audio = true;
        }
      } else if (type == 9) {
        if (v.second.has_sent_video || is_key_frame) {
          v.first->SendMediaData(type, timestamp, payload);
          v.second.has_sent_video = true;
        }
      }
    }
  }

  bool Enter(rtmp::RTMPSession* session) {
    if (!is_alive_) {
      return false;
    }
    State state;
    // send meta
    if (meta_payload_.size()) {
      session->SendMetaData(meta_payload_);
    }
    if (aac_header_payload_.size()) {
      session->SendMediaData(8, 0, aac_header_payload_);
      state.has_sent_audio = true;
    }
    if (cached_video_payload_.size()) {
      for (const auto &p : cached_video_payload_) {
        session->SendMediaData(9, p.first, p.second);
      }
      state.has_sent_video = true;
    }
    return visitors_.insert(std::make_pair(session, state)).second;
  }

  void Leave(rtmp::RTMPSession* session) {
    if (!is_alive_) {
      return;
    }
    visitors_.erase(session);
  }
};

class RoomManager {
  RoomManager() : id_pool_{0, 1, 2, 3}, rooms_(id_pool_.size()) {}
  RoomManager(const RoomManager&) = delete;
  RoomManager& operator=(const RoomManager&) = delete;

  std::unordered_set<int32_t> id_pool_;
  std::vector<Room*> rooms_;

 public:
  static RoomManager& GetInstance() {
    static RoomManager rm;
    return rm;
  }

  // @return 返回负数表示失败，非负数表示 room id。
  int32_t CreateRoom() {
    if (id_pool_.size() <= 0) {
      return -1;
    }
    auto id = *id_pool_.begin();
    id_pool_.erase(id_pool_.begin());
    rooms_[id] = new Room();
    return id;
  }

  void CloseRoom(int32_t room_id) {
    if (room_id < 0 || room_id >= rooms_.size()) {
      LOG_ERROR << "invalid room id " << room_id
                << ", it should be between 0 and " << rooms_.size() - 1;
      return;
    }
    if (id_pool_.count(room_id)) {
      LOG_ERROR << room_id << " room is already closed";
      return;
    }
    delete rooms_[room_id];
    rooms_[room_id] = nullptr;
    id_pool_.insert(room_id);
  }

  bool EnterRoom(int32_t room_id, rtmp::RTMPSession* session) {
    return rooms_[room_id] && rooms_[room_id]->Enter(session);
  }

  void LeaveRoom(int32_t room_id, rtmp::RTMPSession* session) {
    if (rooms_[room_id]) {
      rooms_[room_id]->Leave(session);
    }
  }

  void InitMetaData(int32_t room_id, const std::vector<uint8_t>& mp) {
    rooms_[room_id]->InitMetaData(mp);
  }

  void AddData(int32_t room_id, uint8_t type, uint32_t timestamp,
               const std::vector<uint8_t>& payload) {
    rooms_[room_id]->AddData(type, timestamp, payload);
  }
};

}  // namespace util
}  // namespace live
