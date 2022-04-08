#include "server/net.h"
#include "util/util.h"

namespace live {
namespace util {

void Listener::ListenCallabck(evconnlistener*, evutil_socket_t fd,
                              sockaddr* addr, int len, void* ptr) {
  Listener* listener = reinterpret_cast<Listener*>(ptr);

  std::unique_ptr<Session> session = listener->create_session_();

  if (session.get() == nullptr) {
    LOG_ERROR << "error create session";
    return;
  }

  struct bufferevent* bev = bufferevent_socket_new(listener->event_base_.get(),
                                                   fd, BEV_OPT_CLOSE_ON_FREE);
  if (!bev) {
    LOG_ERROR << "error constructing bufferevent";
    return;
  }

  void* key = reinterpret_cast<void*>(bev);

  session->SetBufferEvent(bev);
  if (!listener->sessions_.insert({key, std::move(session)}).second) {
    LOG_ERROR << "fatal error, duplicate key in sessions_, " << key;
    listener->CloseSession(bev);
    return;
  }

  bufferevent_setcb(bev, ReadCallback, WriteCallback, EventCallback, ptr);
  bufferevent_enable(bev, EV_WRITE | EV_READ);
}

void Listener::ReadCallback(bufferevent* bev, void* ptr) {
  Listener* listener = reinterpret_cast<Listener*>(ptr);

  auto& session = listener->GetSession(bev);
  if (!session) {
    LOG_ERROR << "not found corresponding session";
    listener->CloseSession(bev);
    return;
  }

  struct evbuffer* input = bufferevent_get_input(bev);
  int cnt = evbuffer_get_length(input);

  std::vector<uint8_t>& bytes = session->ReadDataBuffer();

  while (cnt > 0) {
    int now = bytes.size();
    int update = bytes.size() + cnt;
    bytes.resize(update);
    int read_cnt = bufferevent_read(bev, &bytes[0] + now, cnt);
    if (read_cnt < cnt) {
      bytes.resize(update - (cnt - read_cnt));
    }
    cnt = evbuffer_get_length(input);
  }

  // LOG_ERROR << "read data buffer size is " << bytes.size();

  for (size_t pre = 0, cur = session->ReadDataBuffer().size();
       pre != cur && cur;) {
    if (!session->OnRead() || session->IsNeedClose()) {
      LOG_ERROR << "OnRead failed";
      listener->CloseSession(bev);
      return;
    }
    pre = cur;
    cur = session->ReadDataBuffer().size();
  }
  // LOG_ERROR << "after readed data buffer size is " << bytes.size();
}

void Listener::WriteCallback(bufferevent* bev, void* data) {}

void Listener::EventCallback(bufferevent* bev, short events, void* ptr) {
  static std::vector<std::pair<int, std::function<void()>>> handlers = {
      std::make_pair(BEV_EVENT_READING,
                     []() {
                       LOG_ERROR << "error encountered while reading, "
                                 << strerror(errno);
                     }),
      std::make_pair(BEV_EVENT_WRITING,
                     []() {
                       LOG_ERROR << "error encountered while writing, "
                                 << strerror(errno);
                     }),
      std::make_pair(
          BEV_EVENT_EOF,
          []() { LOG_ERROR << "eof file reached, " << strerror(errno); }),
      std::make_pair(BEV_EVENT_ERROR,
                     []() {
                       LOG_ERROR << "unrecoverable error encountered, "
                                 << strerror(errno);
                     }),
      std::make_pair(BEV_EVENT_TIMEOUT,
                     []() {
                       LOG_ERROR << "user-specified timeout reached, "
                                 << strerror(errno);
                     }),
      std::make_pair(BEV_EVENT_CONNECTED,
                     []() {
                       LOG_ERROR << "connect operation finished, "
                                 << strerror(errno);
                     }),
  };

  for (const auto& pr : handlers) {
    if (events & pr.first) {
      pr.second();
    }
  }

  reinterpret_cast<Listener*>(ptr)->CloseSession(bev);
}

}  // namespace util
}  // namespace live
