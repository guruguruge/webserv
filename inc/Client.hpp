#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <unistd.h>  // close
#include <ctime>
#include "Config.hpp"
#include "Defines.hpp"
#include "Http.hpp"

class Client {
 private:
  int _fd;  // 接続済みソケットFD
  std::string _ip;
  int _listenPort;  // どのポートで受けたか（Config検索用）

  int _cgi_pipe_fd;  // CGIからの出力を読むパイプ (初期値 -1)
  pid_t _cgi_pid;    // CGIの子プロセスID (初期値 0)

  ConnState _state;
  time_t _lastActivity;  // タイムアウト判定用

 public:
  // 公開メンバとしてRequest/Responseを持つ（アクセスのしやすさ重視）
  HttpRequest req;
  HttpResponse res;

  Client(int fd, int port, const std::string& ip);
  ~Client();

  int getFd() const;
  ConnState getState() const;
  void setState(ConnState newState);

  // タイムアウト管理
  void updateTimestamp();
  bool isTimedOut(time_t timeout_sec) const;

  // トランザクション完了後のリセット（Keep-Alive対応）
  void reset();
};

#endif
