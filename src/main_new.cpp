/* ************************************************************************** */
/*                                                                            */
/*   main_new.cpp - 新設計に基づくイベントループ                              */
/*                                                                            */
/*   EpollContext の type で switch 分岐し、                                  */
/*   Client が epoll 操作を内部で行う設計                                     */
/*                                                                            */
/* ************************************************************************** */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>

#include "../inc/Client.hpp"
#include "../inc/Config.hpp"
#include "../inc/EpollContext.hpp"
#include "../inc/EpollUtils.hpp"
#include "../inc/RequestHandler.hpp"

// 定数

static const int MAX_EVENTS = 64;
static const int TIMEOUT_MS = 1000;  // epoll_wait タイムアウト
static const time_t CLIENT_TIMEOUT = 60;  // クライアントタイムアウト (秒)
static const int RECV_BUFFER_SIZE = 4096;

// グローバル変数 (シグナルハンドラ用)

static volatile sig_atomic_t g_running = 1;

// ユーティリティ関数

static void signalHandler(int sig) {
  (void)sig;
  g_running = 0;
  std::cout << "\n🛑 Shutting down..." << std::endl;
}

static bool setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

static std::string getClientIp(struct sockaddr_in* addr) {
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
  return std::string(ip);
}

// Listener ソケット作成

static int createListenerSocket(int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    std::cerr << "❌ socket() failed: " << strerror(errno) << std::endl;
    return -1;
  }

  int opt = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "❌ setsockopt() failed: " << strerror(errno) << std::endl;
    close(sock);
    return -1;
  }

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "❌ bind() failed on port " << port << ": " << strerror(errno)
              << std::endl;
    close(sock);
    return -1;
  }

  if (listen(sock, SOMAXCONN) < 0) {
    std::cerr << "❌ listen() failed: " << strerror(errno) << std::endl;
    close(sock);
    return -1;
  }

  if (!setNonBlocking(sock)) {
    std::cerr << "❌ setNonBlocking() failed" << std::endl;
    close(sock);
    return -1;
  }

  std::cout << "✅ Listening on port " << port << std::endl;
  return sock;
}

// イベントハンドラ

static void handleListenerEvent(EpollContext* ctx, int listener_fd,
                                EpollUtils& epoll,
                                std::map<int, Client*>& clients) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int conn_fd = accept(
      listener_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
  if (conn_fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "⚠️ accept() failed: " << strerror(errno) << std::endl;
    }
    return;
  }

  if (!setNonBlocking(conn_fd)) {
    std::cerr << "⚠️ setNonBlocking() failed for client" << std::endl;
    close(conn_fd);
    return;
  }

  std::string ip = getClientIp(&client_addr);
  int port = ctx->listen_port;

  // Client 作成 (内部で epoll.add() が呼ばれる)
  Client* client = new Client(conn_fd, port, ip, &epoll);

  // EpollContext を作成して Client に紐付け
  EpollContext* client_ctx = EpollContext::createClient(client);
  client->setContext(client_ctx);

  // epoll に登録 (EPOLLIN で読み込み待ち)
  epoll.add(conn_fd, client_ctx, EPOLLIN);

  // クライアント管理マップに追加
  clients[conn_fd] = client;

  std::cout << "📥 New connection from " << ip << " (fd=" << conn_fd << ")"
            << std::endl;
}

static void handleClientReadEvent(Client* client, EpollUtils& epoll,
                                  RequestHandler& handler,
                                  std::map<int, Client*>& clients) {
  char buf[RECV_BUFFER_SIZE];
  ssize_t n = recv(client->getFd(), buf, sizeof(buf), 0);

  if (n > 0) {
    client->updateTimestamp();

    // リクエストをフィード (パース)
    bool complete = client->req.feed(buf, static_cast<size_t>(n));

    if (complete) {
      // リクエスト完了 → RequestHandler で処理
      client->setState(PROCESSING);
      handler.handle(client);

      // handle() 内で client->readyToWrite() や client->startCgi() が呼ばれる
      // → epoll の状態変更も Client 内部で完了済み
    }
  } else if (n == 0) {
    // 接続終了
    std::cout << "📤 Connection closed by client (fd=" << client->getFd() << ")"
              << std::endl;
    epoll.del(client->getFd());
    clients.erase(client->getFd());
    delete client->getContext();
    delete client;
  } else {
    // エラー
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "⚠️ recv() error: " << strerror(errno) << std::endl;
      epoll.del(client->getFd());
      clients.erase(client->getFd());
      delete client->getContext();
      delete client;
    }
  }
}

static void handleClientWriteEvent(Client* client, EpollUtils& epoll,
                                   std::map<int, Client*>& clients) {
  const char* data = client->res.getData();
  size_t remaining = client->res.getRemainingSize();

  if (remaining == 0) {
    // 送信完了済み
    return;
  }

  ssize_t sent = send(client->getFd(), data, remaining, 0);

  if (sent > 0) {
    client->updateTimestamp();
    client->res.advance(static_cast<size_t>(sent));

    // 全て送信完了したかチェック
    if (client->res.isDone()) {
      // Keep-Alive チェック
      ConnState state = client->getState();
      if (state == KEEP_ALIVE) {
        // 次のリクエストを待つ
        client->reset();
        client->readyToRead();
        std::cout << "🔄 Keep-Alive: waiting for next request (fd="
                  << client->getFd() << ")" << std::endl;
      } else {
        // 接続終了
        std::cout << "📤 Response sent, closing connection (fd="
                  << client->getFd() << ")" << std::endl;
        epoll.del(client->getFd());
        clients.erase(client->getFd());
        delete client->getContext();
        delete client;
      }
    }
    // まだ残りがある場合は次の EPOLLOUT を待つ
  } else if (sent < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "⚠️ send() error: " << strerror(errno) << std::endl;
      epoll.del(client->getFd());
      clients.erase(client->getFd());
      delete client->getContext();
      delete client;
    }
  }
}

static void handleCgiStdoutEvent(EpollContext* ctx, EpollUtils& epoll) {
  Client* client = ctx->client;
  char buf[RECV_BUFFER_SIZE];
  ssize_t n = read(client->getCgiStdoutFd(), buf, sizeof(buf));

  if (n > 0) {
    client->appendCgiOutput(buf, static_cast<size_t>(n));
  } else if (n == 0) {
    // CGI 完了
    std::cout << "✅ CGI completed (fd=" << client->getFd() << ")" << std::endl;

    // CGI パイプを epoll から削除
    epoll.del(client->getCgiStdoutFd());

    // Client の finishCgi() で後処理 (内部で readyToWrite() が呼ばれる)
    client->finishCgi();

    // CGI 用 Context を解放
    delete ctx;
  } else {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "⚠️ CGI read error: " << strerror(errno) << std::endl;
      epoll.del(client->getCgiStdoutFd());
      client->finishCgi();  // エラーでも後処理
      delete ctx;
    }
  }
}

static void handleCgiStdinEvent(EpollContext* ctx, EpollUtils& epoll) {
  Client* client = ctx->client;

  // POST ボディを CGI に書き込む
  const std::vector<char>& body = client->req.getBody();
  size_t offset = client->getCgiStdinOffset();

  if (body.empty() || offset >= body.size()) {
    // 書き込むデータがない or 全て書き込み完了
    epoll.del(client->getCgiStdinFd());
    close(client->getCgiStdinFd());
    delete ctx;
    return;
  }

  // 残りのデータを書き込み
  size_t remaining = body.size() - offset;
  ssize_t written = write(client->getCgiStdinFd(), &body[offset], remaining);

  if (written > 0) {
    client->advanceCgiStdinOffset(static_cast<size_t>(written));

    // 全て書き込み完了したかチェック
    if (client->getCgiStdinOffset() >= body.size()) {
      // 書き込み完了 → パイプを閉じる
      epoll.del(client->getCgiStdinFd());
      close(client->getCgiStdinFd());
      delete ctx;
    }
    // まだ残りがある場合は次の EPOLLOUT を待つ
  } else if (written < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "⚠️ CGI write error: " << strerror(errno) << std::endl;
      epoll.del(client->getCgiStdinFd());
      close(client->getCgiStdinFd());
      delete ctx;
    }
  }
}

// タイムアウト処理

static void checkTimeouts(std::map<int, Client*>& clients, EpollUtils& epoll) {
  std::map<int, Client*>::iterator it = clients.begin();
  while (it != clients.end()) {
    Client* client = it->second;
    if (client->isTimedOut(CLIENT_TIMEOUT)) {
      std::cout << "⏰ Client timeout (fd=" << client->getFd() << ")"
                << std::endl;
      epoll.del(client->getFd());
      delete client->getContext();
      delete client;
      clients.erase(it++);
    } else {
      ++it;
    }
  }
}

// メインループ

static void eventLoop(EpollUtils& epoll, RequestHandler& handler,
                      std::map<int, Client*>& clients,
                      std::map<int, int>& listener_fds) {
  struct epoll_event events[MAX_EVENTS];

  while (g_running) {
    int nfds = epoll.wait(events, MAX_EVENTS, TIMEOUT_MS);

    if (nfds < 0) {
      if (errno == EINTR) {
        continue;  // シグナル割り込み
      }
      std::cerr << "❌ epoll_wait() failed: " << strerror(errno) << std::endl;
      break;
    }

    // イベント処理
    for (int i = 0; i < nfds; ++i) {
      EpollContext* ctx = static_cast<EpollContext*>(events[i].data.ptr);

      switch (ctx->type) {
        case EpollContext::LISTENER: {
          // 新規接続
          int listener_fd = listener_fds[ctx->listen_port];
          handleListenerEvent(ctx, listener_fd, epoll, clients);
          break;
        }

        case EpollContext::CLIENT: {
          Client* client = ctx->client;
          if (events[i].events & EPOLLIN) {
            handleClientReadEvent(client, epoll, handler, clients);
          } else if (events[i].events & EPOLLOUT) {
            handleClientWriteEvent(client, epoll, clients);
          }
          break;
        }

        case EpollContext::CGI_STDOUT: {
          handleCgiStdoutEvent(ctx, epoll);
          break;
        }

        case EpollContext::CGI_STDIN: {
          handleCgiStdinEvent(ctx, epoll);
          break;
        }
      }
    }

    // タイムアウトチェック
    checkTimeouts(clients, epoll);
  }
}

// メイン関数

int main(int argc, char** argv) {
  // シグナルハンドラ設定
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGPIPE, SIG_IGN);  // SIGPIPE を無視

  // 引数チェック
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
    return 1;
  }

  // 設定読み込み

  MainConfig config;
  // TODO: config.parse(argv[1]);
  // 仮のデフォルト設定
  std::cout << "📄 Loading config: " << argv[1] << std::endl;

  // epoll 初期化

  EpollUtils epoll;

  // Listener ソケット作成

  std::map<int, int> listener_fds;  // port -> fd

  // TODO: config から複数ポートを取得
  // 仮のデフォルトポート
  int default_port = 8080;
  int listener_fd = createListenerSocket(default_port);
  if (listener_fd < 0) {
    return 1;
  }
  listener_fds[default_port] = listener_fd;

  // Listener を epoll に登録
  EpollContext* listener_ctx = EpollContext::createListener(default_port);
  epoll.add(listener_fd, listener_ctx, EPOLLIN);

  // RequestHandler 初期化

  RequestHandler handler(config);

  // Client 管理マップ

  std::map<int, Client*> clients;

  // イベントループ開始

  std::cout << "🚀 Server started. Press Ctrl+C to stop." << std::endl;
  eventLoop(epoll, handler, clients, listener_fds);

  // クリーンアップ

  std::cout << "🧹 Cleaning up..." << std::endl;

  // クライアント解放
  for (std::map<int, Client*>::iterator it = clients.begin();
       it != clients.end(); ++it) {
    delete it->second->getContext();
    delete it->second;
  }
  clients.clear();

  // Listener 解放
  for (std::map<int, int>::iterator it = listener_fds.begin();
       it != listener_fds.end(); ++it) {
    close(it->second);
  }
  delete listener_ctx;

  std::cout << "👋 Server stopped." << std::endl;
  return 0;
}
