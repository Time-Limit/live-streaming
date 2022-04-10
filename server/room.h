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

  std::unordered_set<rtmp::RTMPSession*> visitors_;

 public:
  Room() : is_alive_(true) {}

  ~Room() {
    is_alive_ = false;
  }

  void InitMetaData(const std::vector<uint8_t>& mp) {
    meta_payload_ = mp;
    // broadcast to all visitors
    for (auto v : visitors_) {
      v->SendMetaData(meta_payload_);
    }
  }

  void AddData(uint8_t type, uint32_t timestamp,
               const std::vector<uint8_t>& payload) {
    // broadcast to all visitors
    for (auto v : visitors_) {
      v->SendMediaData(type, timestamp, payload);
    }
  }

  bool Enter(rtmp::RTMPSession* session) {
    if (!is_alive_) {
      return false;
    }
    // send meta
    if (meta_payload_.size()) {
      session->SendMetaData(meta_payload_);
    }
    return visitors_.insert(session).second;
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
