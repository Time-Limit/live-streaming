#pragma once

#include "util/util.h"

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>

namespace live {
namespace util {

class Session {
 public:
  enum FLAG {
    NEED_CLOSE = 0x01,
  };

 private:
  uint32_t flag_ = 0;
  std::vector<uint8_t> read_data_buffer_;
  std::vector<uint8_t> write_data_buffer_;

  bufferevent* be_ = nullptr;

 public:
  bool IsNeedClose() {
    return flag_ & FLAG::NEED_CLOSE;
  }
  void SetFlag(FLAG f) {
    flag_ |= f;
  }
  virtual bool OnRead() {
    return true;
  }
  virtual void OnClose() {}

  bool OnWrite() {
    return Write();
  }

  std::vector<uint8_t>& ReadDataBuffer() {
    return read_data_buffer_;
  }
  std::vector<uint8_t>& WriteDataBuffer() {
    return write_data_buffer_;
  }

  void SetBufferEvent(bufferevent* be) {
    be_ = be;
  }

  bool Write() {
    if (write_data_buffer_.empty()) {
      return true;
    }
    if (bufferevent_write(be_, &write_data_buffer_[0],
                          write_data_buffer_.size())) {
      return false;
    }
    write_data_buffer_.resize(0);
    return true;
  }

  virtual ~Session() {
    bufferevent_free(be_);
  }
};

class Listener {
  static void ListenCallabck(evconnlistener* listener, evutil_socket_t fd,
                             sockaddr* addr, int len, void* ptr);

  static void ReadCallback(bufferevent* bev, void* ptr);

  static void WriteCallback(bufferevent* bev, void* ptr);

  static void EventCallback(bufferevent* bev, short events, void* ptr);

 public:
  using CreateSessionFunc = std::function<std::unique_ptr<Session>()>;

  Listener(int32_t port, CreateSessionFunc cs)
      : port_(port), create_session_(std::move(cs)) {
    if (!create_session_) {
      throw std::string("CreateSessionFunc is not callable");
    }
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port_);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    event_base_.reset(event_base_new());

    if (event_base_.get() == nullptr) {
      throw std::string("event_base_new failed");
    }

    ev_conn_listener_.reset(
        evconnlistener_new_bind(event_base_.get(), ListenCallabck, this,
                                LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
                                (struct sockaddr*)&server, sizeof(server)));

    if (event_base_.get() == nullptr) {
      throw std::string("evconnlistener_new_bind failed");
    }
  }

  void Listen() {
    event_base_dispatch(event_base_.get());
  }

 private:
  struct event_base_deleter {
    void operator()(event_base* ptr) {
      event_base_free(ptr);
    }
  };
  struct evconnlistener_deleter {
    void operator()(evconnlistener* ptr) {
      evconnlistener_free(ptr);
    }
  };

  int32_t port_ = 0;
  std::unique_ptr<event_base, event_base_deleter> event_base_;
  std::unique_ptr<evconnlistener, evconnlistener_deleter> ev_conn_listener_;

  // fd 到 Session* 的映射
  std::unordered_map<void*, std::unique_ptr<Session>> sessions_;

  CreateSessionFunc create_session_;

  // 关闭 bufferevent
  void CloseSession(bufferevent* bev) {
    auto it = sessions_.find(bev);
    if (it != sessions_.end()) {
      it->second->OnClose();
      sessions_.erase(bev);
    }
  }

  std::unique_ptr<Session>& GetSession(void* key) {
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
      return it->second;
    }
    static std::unique_ptr<Session> up;
    return up;
  }
};

}  // namespace util
}  // namespace live
