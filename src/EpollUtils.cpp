#include "../inc/EpollUtils.hpp"
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>

EpollUtils::EpollUtils() {
  this->_epoll_fd = epoll_create(1);
  if (this->_epoll_fd < 0) {
    perror("epoll_create");
    throw std::runtime_error("Failed to create epoll instance");
  }
}

EpollUtils::~EpollUtils() {
  close(this->_epoll_fd);
}

bool EpollUtils::add(int fd, EpollContext* ctx, uint32_t events) {
  struct epoll_event ev;
  std::memset(&ev, 0, sizeof(ev));

  ev.events = events;
  ev.data.ptr = ctx;

  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
    perror("epoll_ctl: add");
    return false;
  }
  return true;
}

bool EpollUtils::mod(int fd, EpollContext* ctx, uint32_t events) {
  struct epoll_event ev;
  std::memset(&ev, 0, sizeof(ev));

  ev.events = events;
  ev.data.ptr = ctx;

  if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
    perror("epoll_ctl: mod");
    return false;
  }
  return true;
}

bool EpollUtils::del(int fd) {
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
    perror("epoll_ctl: del");
    return false;
  }
  return true;
}

int EpollUtils::wait(struct epoll_event* events, int max_events,
                     int timeout_ms) {
  return epoll_wait(_epoll_fd, events, max_events, timeout_ms);
}
