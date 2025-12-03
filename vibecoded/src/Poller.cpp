/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Poller.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Poller.hpp"
#include <algorithm>
#include <stdexcept>

Poller::Poller() {}

Poller::~Poller() {}

void Poller::add(int fd, short events) {
  // 既に存在するかチェック
  if (_fdToIndex.find(fd) != _fdToIndex.end()) {
    throw std::runtime_error("File descriptor already exists in poller");
  }

  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;

  _fds.push_back(pfd);
  _fdToIndex[fd] = _fds.size() - 1;
}

void Poller::modify(int fd, short events) {
  std::map<int, size_t>::iterator it = _fdToIndex.find(fd);
  if (it == _fdToIndex.end()) {
    throw std::runtime_error("File descriptor not found in poller");
  }

  _fds[it->second].events = events;
  _fds[it->second].revents = 0;
}

void Poller::remove(int fd) {
  std::map<int, size_t>::iterator it = _fdToIndex.find(fd);
  if (it == _fdToIndex.end()) {
    return;  // 存在しない場合は何もしない
  }

  size_t index = it->second;

  // vectorから削除（最後の要素と入れ替えて削除）
  if (index < _fds.size() - 1) {
    _fds[index] = _fds[_fds.size() - 1];
  }
  _fds.pop_back();

  // インデックスマップを再構築
  rebuildIndexMap();
}

int Poller::wait(int timeoutMs) {
  if (_fds.empty()) {
    return 0;
  }

  int ret = poll(&_fds[0], _fds.size(), timeoutMs);
  if (ret < 0) {
    throw std::runtime_error("poll() failed");
  }

  return ret;
}

const std::vector<struct pollfd>& Poller::getEvents() const {
  return _fds;
}

void Poller::rebuildIndexMap() {
  _fdToIndex.clear();
  for (size_t i = 0; i < _fds.size(); ++i) {
    _fdToIndex[_fds[i].fd] = i;
  }
}
