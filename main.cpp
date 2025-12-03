#include "inc/Client.hpp"
#include "inc/Config.hpp"
#include "inc/Defines.hpp"
#include "inc/Http.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#define MAX_EVENTS 1024

// epollのdata.ptrで扱うためのラッパー構造体
struct EpollContext {
  int fd;            // 監視対象のFD
  bool is_listener;  // trueならListenSocket, falseならClientSocket
  Client* client;    // Clientの場合のみ有効
  int listen_port;   // Listenerの場合、どのポートか保持

  EpollContext(int f, int port)
      : fd(f), is_listener(true), client(NULL), listen_port(port) {}
  EpollContext(Client* c)
      : fd(c->getFd()), is_listener(false), client(c), listen_port(0) {}
};

// ユーティリティ: ノンブロッキング設定
void setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ---------------------------------------------------------
// [ttanaka担当] リクエストを処理してレスポンスを作る関数 (スタブ)
// 実際にはここで method, path, location設定を見て分岐する
// ---------------------------------------------------------
void processRequest(Client* client, const MainConfig& mainConfig) {
  HttpRequest& req = client->req;
  HttpResponse& res = client->res;

  // 1. Configの検索と紐付け
  // (簡易実装: Hostヘッダは考慮せず、Listenポートだけで検索する例)
  // 本来は req.getHeader("Host") も使う
  const ServerConfig* srvConf = mainConfig.getServer("localhost", 8080);  // 仮
  req.setConfig(srvConf);

  // 2. ロジック実行 GET / POST / DELETE
  if (req.getMethod() == GET) {
    // ルートディレクトリの解決などは LocationConfig を使う
    // const LocationConfig* loc = srvConf->getLocation(req.getPath());

    std::string body =
        "<html><body><h1>Hello from Webserv!</h1><p>Path: " + req.getPath() +
        "</p></body></html>";
    res.setStatusCode(200);
    res.setHeader("Content-Type", "text/html");
    res.setHeader("Content-Length",
                  "wait to build");  // build時に自動計算される想定なら不要
    res.setBody(body);
  } else {
    res.makeErrorResponse(405, srvConf);  // Method Not Allowed
  }

  // 3. 送信準備完了
  res.build();
  client->setState(WRITING_RESPONSE);
}

// Main Loop
int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: ./webserv [config_file]" << std::endl;
    return 1;
  }

  // 設定読み込み パースしないといけない
  MainConfig config;
  if (!config.load(argv[1])) {
    std::cerr << "Error: Failed to load config." << std::endl;
    return 1;
  }

  // epoll作成
  int epoll_fd = epoll_create(1);
  if (epoll_fd < 0) {
    perror("epoll_create");
    return 1;
  }

  // Listen Socketのセットアップ
  // Configに含まれるすべてのポートでListenする
  std::vector<int> listen_fds;
  for (size_t i = 0; i < config.servers.size(); ++i) {
    int port = config.servers[i].listen_port;

    // 重複ポートのチェックは省略(実実装では必要)
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      perror("socket");
      continue;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      perror("bind");
      close(fd);
      continue;
    }
    if (listen(fd, 128) < 0) {
      perror("listen");
      close(fd);
      continue;
    }
    setNonBlocking(fd);

    // epoll登録 (Listenerとして)
    EpollContext* ctx = new EpollContext(fd, port);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = ctx;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
      perror("epoll_ctl listener");
      delete ctx;
      close(fd);
      continue;
    }

    listen_fds.push_back(fd);
    std::cout << "Listening on port " << port << " (FD: " << fd << ")"
              << std::endl;
  }

  // イベントループ
  struct epoll_event events[MAX_EVENTS];
  while (true) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds < 0) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait");
      break;
    }

    for (int i = 0; i < nfds; ++i) {
      EpollContext* ctx = static_cast<EpollContext*>(events[i].data.ptr);

      // 新規接続 (Listener)
      if (ctx->is_listener) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int conn_fd =
            accept(ctx->fd, (struct sockaddr*)&client_addr, &client_len);

        if (conn_fd < 0) {
          if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
          continue;
        }
        setNonBlocking(conn_fd);

        // Client作成 (IPアドレス等は適宜取得)
        Client* new_client = new Client(conn_fd, ctx->listen_port, "127.0.0.1");

        // epoll登録 (Clientとして)
        EpollContext* client_ctx = new EpollContext(new_client);
        struct epoll_event ev_client;
        ev_client.events = EPOLLIN;  // まずはリクエストを読む
        ev_client.data.ptr = client_ctx;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev_client) < 0) {
          perror("epoll_ctl client");
          delete new_client;
          delete client_ctx;
          close(conn_fd);
        }
        // std::cout << "New Connection: " << conn_fd << std::endl;
      }
      // 既存クライアント (Client)
      else {
        Client* client = ctx->client;

        // 読み込み (Request)
        if (events[i].events & EPOLLIN) {
          char buf[4096];  // 少し大きめに
          ssize_t n = recv(client->getFd(), buf, sizeof(buf), 0);

          if (n <= 0) {
            // 切断またはエラー
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->getFd(), NULL);
            delete client;
            delete ctx;  // コンテキストも削除
            continue;
          }

          // パーサーへ投入 sabe担当
          if (client->req.feed(buf, n)) {
            // パース完了 -> ステータス変更 -> ロジック処理
            client->setState(PROCESSING);

            // ttanaka担当 (ここでResponseが作られる)
            processRequest(client, config);

            // 監視イベントを書き込み(EPOLLOUT)に変更
            struct epoll_event ev_mod;
            ev_mod.events = EPOLLOUT;
            ev_mod.data.ptr = ctx;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->getFd(), &ev_mod);
          }
        }
        // 書き込み (Response)
        else if (events[i].events & EPOLLOUT) {
          const char* data = client->res.getData();
          size_t len = client->res.getRemainingSize();

          ssize_t n = send(client->getFd(), data, len, 0);
          if (n < 0) {
            // 送信エラー処理...
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->getFd(), NULL);
            delete client;
            delete ctx;
            continue;
          }

          client->res.advance(n);

          if (client->res.isDone()) {
            // 送信完了
            // Keep-AliveかCloseかで分岐 (ヘッダを見る実装が必要)
            // ここでは簡易的にCloseする例
            // Keep-Aliveなら client->reset() して EPOLLIN に戻す

            // std::cout << "Response Sent. Closing." << std::endl;
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->getFd(), NULL);
            delete client;
            delete ctx;
          }
        }
      }
    }
  }

  // cleanup...
  return 0;
}