#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <sys/types.h>  // pid_t
#include <unistd.h>     // close
#include <ctime>
#include <string>
#include "Config.hpp"
#include "Defines.hpp"
#include "Http.hpp"

// 前方宣言 (循環参照回避)
class EpollUtils;
struct EpollContext;

/*
 * Client Class
 * 責務:
 * 1. 接続済みソケットの管理
 * 2. HTTP リクエスト/レスポンスの保持
 * 3. 状態遷移の管理 (ConnState)
 * 4. epoll イベントの操作 (EpollUtils 経由)
 * 5. CGI 関連情報の管理
 */
class Client {
 public:
  // 公開メンバとしてRequest/Responseを持つ（アクセスのしやすさ重視）
  HttpRequest req;
  HttpResponse res;

  Client(int fd, int port, const std::string& ip, EpollUtils* epoll);
  ~Client();

  // --- 基本情報 ---
  int getFd() const;
  int getListenPort() const;
  const std::string& getIp() const;

  // --- 状態管理 ---
  ConnState getState() const;
  void setState(ConnState newState);

  // --- タイムアウト管理 ---
  void updateTimestamp();
  bool isTimedOut(time_t timeout_sec) const;

  // --- 状態遷移メソッド (epoll 操作を内部で行う) ---
  // RequestHandler はこれらを呼ぶだけで OK

  void readyToWrite();  // WRITING_RESPONSE へ遷移 + EPOLLOUT 設定
  void readyToRead();   // リクエスト待ちへ遷移 + EPOLLIN 設定 (Keep-Alive)
  void startCgi(const std::string& scriptPath);  // CGI 実行開始
  void finishCgi();                              // CGI 完了処理
  void markClose();                              // 接続終了マーク

  // --- CGI 情報アクセス (main.cpp から使用) ---
  pid_t getCgiPid() const;
  int getCgiStdoutFd() const;
  int getCgiStdinFd() const;
  void appendCgiOutput(const char* buf, size_t len);
  const std::string& getCgiOutput() const;

  // CGI stdin オフセット管理 (部分書き込み対応)
  size_t getCgiStdinOffset() const;
  void advanceCgiStdinOffset(size_t bytes);

  // CGI設定用セッター (RequestHandlerから使用)
  void setCgiPid(pid_t pid);
  void setCgiStdinFd(int fd);
  void setCgiStdoutFd(int fd);

  // --- Context 管理 ---
  void setContext(EpollContext* ctx);
  EpollContext* getContext() const;

  // --- トランザクション完了後のリセット（Keep-Alive対応）---
  void reset();

 private:
  int _fd;  // 接続済みソケットFD
  std::string _ip;
  int _listenPort;  // どのポートで受けたか（Config検索用）

  EpollUtils* _epoll;      // epoll 操作用 (参照)
  EpollContext* _context;  // 自身の EpollContext

  ConnState _state;
  time_t _lastActivity;  // タイムアウト判定用

  // --- CGI 関連 ---
  pid_t _cgi_pid;           // CGI の子プロセス ID (初期値 -1)
  int _cgi_stdout_fd;       // CGI stdout パイプ (初期値 -1)
  int _cgi_stdin_fd;        // CGI stdin パイプ (初期値 -1)
  std::string _cgi_output;  // CGI 出力バッファ
  size_t
      _cgi_stdin_offset;  // CGI stdin 書き込み済みオフセット (部分書き込み対応)

  // --- CGI 内部ヘルパー ---
  void _cleanupCgi();

  // Orthodox Canonical Form (コピー禁止)
  Client(const Client&);
  Client& operator=(const Client&);
};

#endif
