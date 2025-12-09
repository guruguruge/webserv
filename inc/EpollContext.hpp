#ifndef EPOLLCONTEXT_HPP
#define EPOLLCONTEXT_HPP

#include "Client.hpp"

struct EpollContext {
  int fd;  // 監視対象のFD
  enum FdType { LISTENER, CLIENT_SOCKET, CGI_PIPE };
  bool is_listener;  // trueならListenSocket, falseならClientSocket
  Client* client;    // Clientの場合のみ有効
  int listen_port;   // Listenerの場合、どのポートか保持

  EpollContext(int f, int port)
      : fd(f), is_listener(true), client(NULL), listen_port(port) {}
  EpollContext(Client* c)
      : fd(c->getFd()), is_listener(false), client(c), listen_port(0) {}
};

#endif