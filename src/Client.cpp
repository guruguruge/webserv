/* ************************************************************************** */
/*                                                                            */
/*   Client.cpp - クライアント接続管理クラス                                  */
/*                                                                            */
/*   責務:                                                                    */
/*   - 接続済みソケットの管理                                                 */
/*   - HTTP リクエスト/レスポンスの保持                                       */
/*   - 状態遷移の管理                                                         */
/*   - epoll イベントの操作 (EpollUtils 経由)                                 */
/*   - CGI 関連情報の管理                                                     */
/*                                                                            */
/* ************************************************************************** */

#include "../inc/Client.hpp"
#include <sys/epoll.h>
#include <sys/wait.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include "../inc/EpollContext.hpp"
#include "../inc/EpollUtils.hpp"

// ========================================
// コンストラクタ / デストラクタ
// ========================================

Client::Client(int fd, int port, const std::string& ip, EpollUtils* epoll)
    : _fd(fd),
      _ip(ip),
      _listenPort(port),
      _epoll(epoll),
      _context(NULL),
      _state(READING_REQUEST),
      _lastActivity(std::time(NULL)),
      _cgi_pid(-1),
      _cgi_stdout_fd(-1),
      _cgi_stdin_fd(-1),
      _cgi_output(),
      _cgi_stdin_offset(0) {}

Client::~Client() {
  _cleanupCgi();
  if (_fd >= 0) {
    close(_fd);
  }
}

// ========================================
// 基本情報アクセサ
// ========================================

int Client::getFd() const {
  return _fd;
}

int Client::getListenPort() const {
  return _listenPort;
}

const std::string& Client::getIp() const {
  return _ip;
}

// ========================================
// 状態管理
// ========================================

ConnState Client::getState() const {
  return _state;
}

void Client::setState(ConnState newState) {
  _state = newState;
}

// ========================================
// タイムアウト管理
// ========================================

void Client::updateTimestamp() {
  _lastActivity = std::time(NULL);
}

bool Client::isTimedOut(time_t timeout_sec) const {
  return (std::time(NULL) - _lastActivity) > timeout_sec;
}

// ========================================
// 状態遷移メソッド (epoll 操作を内部で行う)
// ========================================

void Client::readyToWrite() {
  _state = WRITING_RESPONSE;
  if (_epoll && _context) {
    _epoll->mod(_fd, _context, EPOLLOUT);
  }
}

void Client::readyToRead() {
  _state = READING_REQUEST;
  if (_epoll && _context) {
    _epoll->mod(_fd, _context, EPOLLIN);
  }
}

void Client::startCgi(const std::string& scriptPath) {
  (void)scriptPath;  // TODO: 実際の CGI 実行ロジック
  _state = WAITING_CGI;
  _cgi_stdin_offset = 0;  // オフセットをリセット
  // CGI パイプの epoll 登録は RequestHandler 内で行われる
}

void Client::finishCgi() {
  // CGI 出力を使ってレスポンスを構築
  // (実際のパース処理は RequestHandler で行う想定)
  _state = WRITING_RESPONSE;
  if (_epoll && _context) {
    _epoll->mod(_fd, _context, EPOLLOUT);
  }
  _cleanupCgi();
}

void Client::markClose() {
  _state = CLOSE_CONNECTION;
}

// ========================================
// CGI 情報アクセサ
// ========================================

pid_t Client::getCgiPid() const {
  return _cgi_pid;
}

int Client::getCgiStdoutFd() const {
  return _cgi_stdout_fd;
}

int Client::getCgiStdinFd() const {
  return _cgi_stdin_fd;
}

void Client::appendCgiOutput(const char* buf, size_t len) {
  _cgi_output.append(buf, len);
}

const std::string& Client::getCgiOutput() const {
  return _cgi_output;
}

// CGI stdin オフセット管理 (部分書き込み対応)
size_t Client::getCgiStdinOffset() const {
  return _cgi_stdin_offset;
}

void Client::advanceCgiStdinOffset(size_t bytes) {
  _cgi_stdin_offset += bytes;
}

// ========================================
// Context 管理
// ========================================

void Client::setContext(EpollContext* ctx) {
  _context = ctx;
}

EpollContext* Client::getContext() const {
  return _context;
}

// ========================================
// トランザクションリセット (Keep-Alive 対応)
// ========================================

void Client::reset() {
  req.clear();
  res.clear();
  _state = READING_REQUEST;
  _cgi_pid = -1;
  _cgi_stdout_fd = -1;
  _cgi_stdin_fd = -1;
  _cgi_output.clear();
  _cgi_stdin_offset = 0;
  updateTimestamp();
}

// ========================================
// プライベートヘルパー
// ========================================

void Client::_cleanupCgi() {
  if (_cgi_stdout_fd >= 0) {
    close(_cgi_stdout_fd);
    _cgi_stdout_fd = -1;
  }
  if (_cgi_stdin_fd >= 0) {
    close(_cgi_stdin_fd);
    _cgi_stdin_fd = -1;
  }
  if (_cgi_pid > 0) {
    // 子プロセスを待つ (ゾンビ回避)
    int status;
    waitpid(_cgi_pid, &status, WNOHANG);
    _cgi_pid = -1;
  }
  _cgi_output.clear();
  _cgi_stdin_offset = 0;
}
