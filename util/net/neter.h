#ifndef _NETER_H_
#define _NETER_H_

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>

#include "exptype.h"
#include "lock.h"
#include "log.h"
#include "octets.h"
#include "threadpool.h"

namespace TCORE {

class Neter {
 public:
  static const std::string CA_FILE_PATH;

  struct Callback {
    template <typename PROTOCOL>
    struct Response {
      typedef std::function<void(PROTOCOL&&, session_id_t)> Func;
    };
    struct Connect {
      typedef std::function<void(session_id_t /*new sid*/)> Func;
    };
    struct Disconnect {
      typedef std::function<void(session_id_t /*old sid*/,
                                 const std::string& /*ip*/, int /*port*/)>
          Func;
    };
  };

  template <typename PROTOCOL>
  struct GenerateProtocol {
    static void Generate(session_id_t sid, Octets& data,
                         typename Callback::Response<PROTOCOL>::Func rcb) {
      OctetsStream os(data);
      PROTOCOL p;
      try {
        for (;;) {
          os >> OctetsStream::START >> p >> OctetsStream::COMMIT;
          rcb(std::move(p), sid);
          Log::Debug(
              "Neter::GenerateProtocol::Generate, deserialize success !!!");
        }
      } catch (...) {
        Log::Debug(
            "Neter::GenerateProtocol::Generate, deserialize throw exception "
            "!!!");
        os >> OctetsStream::REVERT;
      }
      data = os.GetData();
    }
  };

 private:
  class Session;
  typedef std::shared_ptr<Session> SessionPtr;
  friend class Session;
  class Session {
    friend class Neter;

    // BASE DATA/METHOD BEGIN

    enum SESSION_TYPE {
      INVALID_SESSION = -1,

      ACCEPTOR_SESSION = 0,
      CONNECTOR_SESSION,
      EXCHANGE_SESSION,
    };

    Session(session_id_t sid, SESSION_TYPE type, int fd);

   public:
    ~Session();

   private:
    void Close();

    session_id_t sid;
    int fd;
    SESSION_TYPE type;

    bool IsInitSuccess() const {
      return type != INVALID_SESSION;
    }

    SESSION_TYPE GetType() const {
      return type;
    }
    int GetFD() const {
      return fd;
    }
    session_id_t GetSID() const {
      return sid;
    }

    // BASE DATA/METHOD END

    // CALLBACK PART BEGIN
    Callback::Connect::Func connect_callback;

    Callback::Disconnect::Func disconnect_callback;

    typedef std::function<void(session_id_t sid, Octets&)> DeserializeFunc;
    DeserializeFunc deserialize;

   public:
    void SetConnectCallback(Callback::Connect::Func callback) {
      connect_callback = callback;
    }
    void SetDisconnectCallback(Callback::Disconnect::Func dcb) {
      disconnect_callback = dcb;
    }
    bool InitDeserializeFunc(DeserializeFunc df) {
      if (deserialize) return false;
      return deserialize = df, true;
    }

    // CALLBACK PART END

    // HANDLE READ/WRITE PART BEGIN

    enum EVENT_FLAG {
      READ_ACCESS = 0x1,
      READ_PENDING = 0x2,
      WRITE_ACCESS = 0x4,
      WRITE_READY = 0x8,
      WRITE_PENDING = 0x10,
      CLOSE_READY = 0x20,

      EMPTY_EVENT_FLAG = 0x0,
      EVENT_FLAG_MASK = READ_ACCESS | READ_PENDING | WRITE_ACCESS |
                        WRITE_READY | WRITE_PENDING | CLOSE_READY,
    };

    mutable SpinLock event_flag_lock;
    typedef unsigned char event_flag_t;
    event_flag_t event_flag;

    bool TestAndModifyEventFlag(event_flag_t test, event_flag_t except,
                                event_flag_t set, event_flag_t clear);
    bool TestEventFlag(event_flag_t e) const;
    void SetEventFlag(event_flag_t e);
    void ClrEventFlag(event_flag_t e);

    static void Read(SessionPtr ptr);
    Octets read_data;

    static void Write(SessionPtr ptr);
    typedef std::list<Octets> SendDataList;
    SendDataList send_data_list;
    mutable SpinLock send_data_list_lock;
    size_t cursor_of_first_send_data;
    static bool AppendSendData(SessionPtr ptr, const Octets& data);

    typedef void (Session::*InnerReadFuncPtr)();
    InnerReadFuncPtr read_func_ptr;
    void DefaultReadFunc() {
      Log::Error("Session::DefaultReadFunc, you should never call me.");
    }
    void AcceptorReadFunc();
    void ExchangerReadFunc();

    typedef void (Session::*InnerWriteFuncPtr)();
    InnerWriteFuncPtr write_func_ptr;
    void DefaultWriteFunc() {
      Log::Error("Session::DefaultWriteFunc, you should never call me.");
    }
    void ExchangerWriteFunc();
    void ConnectorWriteFunc();

    // HANDLE READ/WRITE PART END

   private:
    std::string ip;
    int port;

    void SetIP(const std::string& _ip) {
      ip = _ip;
    }
    void SetPort(int _port) {
      port = _port;
    }

   public:
    const std::string& GetIP() const {
      return ip;
    }
    int GetPort() const {
      return port;
    }
  };
  // session end

 private:
  struct InitSignalHandleTask : public Task {
    InitSignalHandleTask() : Task(RDWR_TASK) {}
    void Exec() {
      sigset_t set;
      sigemptyset(&set);
      sigaddset(&set, SIGPIPE);
      if (pthread_sigmask(SIG_BLOCK, &set, NULL)) {
        Log::Error("InitSignalHandleTask::Exec, block pipe fialed !!!");
      }
    }
  };

  struct SessionWriteTask : public Task {
    SessionWriteTask(SessionPtr p) : Task(RDWR_TASK), ps(p) {}
    void Exec() {
      Session::Write(ps);
    }

   private:
    SessionPtr ps;
  };

  struct SessionReadTask : public Task {
    SessionReadTask(SessionPtr p) : Task(RDWR_TASK), ps(p) {}
    void Exec() {
      Session::Read(ps);
    }

   private:
    SessionPtr ps;
  };

  struct NeterPollTask : public Task {
    NeterPollTask() : Task(POLL_TASK) {}
    void Exec() {
      while (true) {
        Neter::GetInstance().Wait();
        pthread_testcancel();
      }
    }
  };

  static void TryAddReadTask(SessionPtr ptr);
  static void TryAddWriteTask(SessionPtr ptr);

  mutable SpinLock session_container_lock;
  typedef std::map<session_id_t, SessionPtr> SessionContainer;
  SessionContainer session_container;

  SessionPtr GetSession(session_id_t sid) const;

  typedef std::list<session_id_t> SessionKeyList;
  SessionKeyList ready_close_session_list;
  SpinLock ready_close_session_list_lock;

 private:
  enum {
    POLL_TASK = 0,
    RDWR_TASK = 1,

    THREAD_COUNT = 2,
  };
  ThreadPool threadpool;

  enum {
    EPOLL_EVENT_SIZE = 1024,
  };
  struct epoll_event events[EPOLL_EVENT_SIZE];

  int epoll_instance_fd;

  Neter();
  ~Neter();

  bool Ctrl(int op, int fd, struct epoll_event* event);

  void Wait(time_t timeout = 1000);

  SpinLock session_id_spawner_lock;
  session_id_t session_id_spawner;

  session_id_t GenerateSessionID() {
    SpinLockGuard guard(session_id_spawner_lock);
    return ++session_id_spawner;
  }

 public:
  void AddReadyCloseSession(session_id_t sid) {
    SpinLockGuard guard(ready_close_session_list_lock);
    ready_close_session_list.push_back(sid);
  }

  template <typename PROTOCOL>
  static bool Listen(const char* ip, int port, Callback::Connect::Func ccb,
                     Callback::Disconnect::Func dcb,
                     typename Callback::Response<PROTOCOL>::Func rcb);

  template <typename PROTOCOL>
  static bool Connect(const std::string& ip, int port,
                      Callback::Connect::Func ccb,
                      Callback::Disconnect::Func dcb,
                      typename Callback::Response<PROTOCOL>::Func rcb);

  template <typename PROTOCOL>
  static bool SendProtocol(session_id_t sid, const PROTOCOL& protocol);

  static Neter& GetInstance() {
    static Neter n;
    return n;
  }
  bool IsInitSuccess() {
    return epoll_instance_fd != -1;
  }
};

template <typename PROTOCOL>
bool Neter::SendProtocol(session_id_t sid, const PROTOCOL& protocol) {
  try {
    SessionPtr ptr = Neter::GetInstance().GetSession(sid);
    if (!ptr) {
      Log::Error("Neter::SendProtocol, sid=", sid, " ,session not found !!!");
      return false;
    }
    OctetsStream os;
    os << protocol;
    bool res = Session::AppendSendData(ptr, os.GetData());
    return res;
  } catch (...) {
    Log::Error("Neter::SendProtocol, sid=", sid, " ,serialize failed !!!");
  }

  return false;
}

template <typename PROTOCOL>
bool Neter::Connect(const std::string& ip, int port,
                    Callback::Connect::Func ccb, Callback::Disconnect::Func dcb,
                    typename Callback::Response<PROTOCOL>::Func rcb) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    Log::Error("Neter::Connect, ip=", ip, " ,port=", port,
               " ,info=", strerror(errno));
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip.c_str());

  fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

  SessionPtr ptr(new Session(Neter::GetInstance().GenerateSessionID(),
                             Session::CONNECTOR_SESSION, sock));
  ptr->SetConnectCallback(ccb);
  ptr->SetDisconnectCallback(dcb);
  ptr->SetIP(ip);
  ptr->SetPort(port);
  ptr->InitDeserializeFunc([rcb](session_id_t sid, Octets& data) -> void {
    GenerateProtocol<PROTOCOL>::Generate(sid, data, rcb);
  });

  if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
    if (errno == EINPROGRESS) {
      epoll_event ev;
      ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
      ev.data.u64 = ptr->GetSID();

      ptr->read_func_ptr = &Session::ExchangerReadFunc;

      ptr->SetEventFlag(Session::WRITE_READY);
      Neter::GetInstance().session_container.insert(
          std::make_pair(ptr->GetSID(), ptr));
      Neter::GetInstance().Ctrl(EPOLL_CTL_ADD, ptr->GetFD(), &ev);
      return true;
    }
    Log::Error("Neter::Connect, ip=", ip, " ,port=", port,
               ", connect failed, info=", strerror(errno));
    return false;
  }

  Log::Trace("Neter::Connect, connect success, ip=", ip, ", port=", port);

  ptr->write_func_ptr = &Session::ExchangerWriteFunc;
  ptr->read_func_ptr = &Session::ExchangerReadFunc;

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
  ev.data.u64 = ptr->GetSID();

  Neter::GetInstance().session_container.insert(
      std::make_pair(ptr->GetSID(), ptr));
  Neter::GetInstance().Ctrl(EPOLL_CTL_ADD, ptr->GetFD(), &ev);
  if (ccb) {
    ccb(ptr->GetSID());
  }
  return true;
}

template <typename PROTOCOL>
bool Neter::Listen(const char* ip, int port, Callback::Connect::Func ccb,
                   Callback::Disconnect::Func dcb,
                   typename Callback::Response<PROTOCOL>::Func rcb) {
  if (!ip) {
    Log::Error("Neter::Listen, invalid ip address.");
    return false;
  }

  int sockfd = 0;
  int optval = -1;
  struct sockaddr_in server;
  socklen_t socklen = sizeof(struct sockaddr_in);

  sockfd = socket(PF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    Log::Error("Neter::Listen, socket, error=", strerror(errno));
    return false;
  }

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
  memset(&server, 0, socklen);
  struct in_addr address;
  if (inet_pton(AF_INET, ip, &address) == -1) {
    Log::Error("Neter::inet_pton, ip=", ip, ", error=", strerror(errno));
    return false;
  }
  server.sin_addr.s_addr = address.s_addr;
  server.sin_port = htons(port);
  server.sin_family = AF_INET;

  if (bind(sockfd, (struct sockaddr*)&server, socklen) < 0) {
    Log::Error("Neter::Listen, bind, error=", strerror(errno));
    return false;
  }

  listen(sockfd, 0);
  Log::Debug("Neter::Listen, fd=", sockfd);

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  SessionPtr ptr(new Session(Neter::GetInstance().GenerateSessionID(),
                             Session::ACCEPTOR_SESSION, sockfd));
  ptr->SetIP(ip);
  ptr->SetPort(port);
  ptr->SetConnectCallback(ccb);
  ptr->SetDisconnectCallback(dcb);
  ptr->InitDeserializeFunc([rcb](session_id_t sid, Octets& data) -> void {
    GenerateProtocol<PROTOCOL>::Generate(sid, data, rcb);
  });
  Neter::GetInstance().session_container.insert(
      std::make_pair(ptr->GetSID(), ptr));
  ev.data.u64 = ptr->GetSID();
  Neter::GetInstance().Ctrl(EPOLL_CTL_ADD, ptr->GetFD(), &ev);

  return true;
}

}  // namespace TCORE

namespace {
struct NeterInit {
  NeterInit() {
    if (TCORE::Neter::GetInstance().IsInitSuccess() == false) {
      throw "init neter failed !!!";
    }
  }
};
NeterInit _neter_init_;
}  // namespace

#endif
