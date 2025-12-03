/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Listener.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

Listener::Listener(const std::string& host, int port, ServerConfig* config)
    : _fd(-1), _host(host), _port(port), _config(config) {}

Listener::~Listener() {
  if (_fd != -1) {
    close(_fd);
  }
}

void Listener::init() {
  createSocket();
  setReuseAddr();
  setNonBlocking();
  bindSocket();
  listenSocket();

  std::cout << "✅ Listener initialized on " << _host << ":" << _port
            << " (fd: " << _fd << ")" << std::endl;
}

int Listener::getFd() const {
  return _fd;
}

ServerConfig* Listener::getServerConfig() const {
  return _config;
}

std::string Listener::getHost() const {
  return _host;
}

int Listener::getPort() const {
  return _port;
}

void Listener::createSocket() {
  _fd = socket(AF_INET, SOCK_STREAM, 0);
  if (_fd < 0) {
    throw std::runtime_error("Failed to create socket");
  }
}

void Listener::setNonBlocking() {
  // macOS用の非ブロッキング設定
  // F_SETFL と O_NONBLOCK のみ使用（subject の要件に従う）
  int flags = fcntl(_fd, F_GETFL, 0);
  if (flags == -1) {
    throw std::runtime_error("Failed to get socket flags");
  }

  if (fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::runtime_error("Failed to set non-blocking mode");
  }
}

void Listener::setReuseAddr() {
  int optval = 1;
  if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    throw std::runtime_error("Failed to set SO_REUSEADDR");
  }
}

void Listener::bindSocket() {
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);

  // ホストアドレスの設定
  if (_host.empty() || _host == "0.0.0.0") {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) <= 0) {
      throw std::runtime_error("Invalid host address: " + _host);
    }
  }

  if (bind(_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    std::ostringstream oss;
    oss << "Failed to bind to " << _host << ":" << _port;
    throw std::runtime_error(oss.str());
  }
}

void Listener::listenSocket() {
  // バックログは128（一般的な値）
  if (listen(_fd, 128) < 0) {
    throw std::runtime_error("Failed to listen on socket");
  }
}
