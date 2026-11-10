#include "bell/net/SocketPollListener.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <utility>

#include "bell/Logger.h"

using namespace bell::net;

static constexpr auto LOG_TAG = "SocketPollListener";

SocketPollListener::SocketPollListener(bool registerWakeSocket) {

  if (registerWakeSocket) {
    // Only register wake socket when requested
    wakeFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (wakeFd_ < 0) {
      BELL_LOG(error, LOG_TAG, "wake socket create failed: {}",
               strerror(errno));
      return;
    }

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bindAddr.sin_port = 0;  // kernel-assigned ephemeral port
    if (::bind(wakeFd_, reinterpret_cast<const sockaddr*>(&bindAddr),
               sizeof(bindAddr)) < 0) {
      BELL_LOG(error, LOG_TAG, "wake socket bind failed: {}", strerror(errno));
      ::close(wakeFd_);
      wakeFd_ = -1;
      return;
    }

    sockaddr_in boundAddr{};
    socklen_t addrLen = sizeof(boundAddr);
    if (::getsockname(wakeFd_, reinterpret_cast<sockaddr*>(&boundAddr),
                      &addrLen) < 0) {
      BELL_LOG(error, LOG_TAG, "wake socket getsockname failed: {}",
               strerror(errno));
      ::close(wakeFd_);
      wakeFd_ = -1;
      return;
    }
    wakePort_ = ntohs(boundAddr.sin_port);
  }

  ::fcntl(wakeFd_, F_SETFL, O_NONBLOCK);

  std::scoped_lock lock(pollMutex);
  updateFdListLocked();  // Seed fds so poll() watches wakeFd_ immediately.
}

SocketPollListener::~SocketPollListener() {
  // Callbacks can refer to the handlers by itself
  // so we swap it making handlers valid while remaining is cleared
  std::unordered_map<int, Handler> remaining;
  {
    std::scoped_lock lock(pollMutex);
    remaining.swap(handlers);
  }
  remaining.clear();

  if (wakeFd_ >= 0)
    ::close(wakeFd_);
}

void SocketPollListener::wake() {
  if (wakeFd_ < 0)
    return;

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  dst.sin_port = htons(wakePort_);

  char byte = 1;
  // Non-blocking. If the kernel's receive queue is already full, poll
  // will wake on the existing data anyway — dropping this byte is fine.
  (void)::sendto(wakeFd_, &byte, 1, 0, reinterpret_cast<const sockaddr*>(&dst),
                 sizeof(dst));
}

void SocketPollListener::drainWakeFd() {
  if (wakeFd_ < 0)
    return;
  char buf[64];
  while (::recv(wakeFd_, buf, sizeof(buf), 0) > 0) {}
}

SocketPollListener::Registration::Registration(Registration&& other) noexcept
    : listener_(other.listener_),
      socket_(std::move(other.socket_)),
      event_(other.event_) {
  other.listener_ = nullptr;
}

SocketPollListener::Registration& SocketPollListener::Registration::operator=(
    Registration&& other) noexcept {
  if (this != &other) {
    reset();
    listener_ = other.listener_;
    socket_ = std::move(other.socket_);
    event_ = other.event_;
    other.listener_ = nullptr;
  }
  return *this;
}

void SocketPollListener::Registration::reset() {
  if (listener_ != nullptr && socket_) {
    listener_->unregister(socket_->getFd(), event_);
  }
  listener_ = nullptr;
  socket_.reset();
}

SocketPollListener::Registration SocketPollListener::watch(
    std::shared_ptr<Socket> socket, Event polledEvent, EventCallback onEvent) {
  if (!socket || !socket->isValid()) {
    throw std::invalid_argument("Invalid socket");
  }

  {
    // A replaced callback is destroyed after the lock, see unregister()
    EventCallback replaced;

    std::scoped_lock lock(pollMutex);
    int fd = socket->getFd();
    auto& handler = handlers[fd];
    if (!handler.socket)
      handler.socket = socket;
    auto& slot = handler.callbacks[polledEvent];
    replaced = std::exchange(slot, std::move(onEvent));
    updateFdListLocked();
  }

  // Wake any currently-blocked poll so it re-reads fds and includes the
  // new watch immediately.
  wake();

  return Registration{this, std::move(socket), polledEvent};
}

void SocketPollListener::unregister(int fd, Event event) {
  // Destroy after the lock is released
  EventCallback removed;
  std::shared_ptr<Socket> removedSocket;

  std::scoped_lock lock(pollMutex);
  auto it = handlers.find(fd);
  if (it == handlers.end())
    return;
  auto cb = it->second.callbacks.find(event);
  if (cb != it->second.callbacks.end()) {
    removed = std::move(cb->second);
    it->second.callbacks.erase(cb);
  }
  if (it->second.callbacks.empty()) {
    removedSocket = std::move(it->second.socket);
    handlers.erase(it);
  }
  updateFdListLocked();
}

void SocketPollListener::updateFdListLocked() {
  fds.clear();
  for (const auto& [fd, handler] : handlers) {
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = 0;
    for (const auto& [event, _] : handler.callbacks) {
      pfd.events |= static_cast<short>(event);
    }
    fds.push_back(pfd);
  }
  if (wakeFd_ >= 0) {
    pollfd pfd{};
    pfd.fd = wakeFd_;
    pfd.events = POLLIN;
    fds.push_back(pfd);
  }
}

void SocketPollListener::poll(
    std::optional<std::chrono::milliseconds> timeout) {
  std::vector<pollfd> fdsCopy;
  {
    std::scoped_lock lock(pollMutex);
    fdsCopy.assign(fds.begin(), fds.end());
  }

  int timeoutMs = timeout ? static_cast<int>(timeout->count()) : -1;
  int pollResult = ::poll(fdsCopy.data(), fdsCopy.size(), timeoutMs);

  if (pollResult < 0) {
    if (errno != EINTR) {
      BELL_LOG(debug, LOG_TAG, "poll failed err={}", strerror(errno));
    }
    return;
  }

  for (const auto& pfd : fdsCopy) {
    if (pfd.revents == 0)
      continue;
    if (pfd.fd == wakeFd_) {
      drainWakeFd();
      continue;
    }

    std::shared_ptr<Socket> socket;
    std::vector<std::pair<Event, EventCallback>> toInvoke;
    {
      std::scoped_lock lock(pollMutex);
      auto it = handlers.find(pfd.fd);
      if (it == handlers.end())
        continue;
      socket = it->second.socket;
      for (const auto& [ev, cb] : it->second.callbacks) {
        if (pfd.revents & static_cast<short>(ev)) {
          toInvoke.emplace_back(ev, cb);
        }
      }
    }

    for (auto& [ev, cb] : toInvoke) {
      try {
        cb(*socket);
      } catch (const std::exception& e) {
        BELL_LOG(error, LOG_TAG, "callback threw: {}", e.what());
      } catch (...) {
        BELL_LOG(error, LOG_TAG, "callback threw unknown");
      }
    }
  }
}
