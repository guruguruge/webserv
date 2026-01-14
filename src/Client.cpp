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

namespace {

// Converts a value to a string using stringstream.
// This is a C++98 compatible alternative to std::to_string.
//
// Args:
//   value: The value to convert.
//
// Returns:
//   The string representation of the value.
template <typename T>
std::string toString(const T& value) {
  std::ostringstream oss;
  oss << value;
  if (oss.fail()) {
    return "";
  }
  return oss.str();
}

std::string toEnvKey(const std::string& headerKey) {
  std::string result = headerKey;
  for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
    if (*it == '-') {
      *it = '_';
    } else {
      *it = std::toupper(static_cast<unsigned char>(*it));
    }
  }
  return result;
}

char** createCgiEnv(const Client& client, const std::string& realPath) {
  const HttpRequest& req = client.req;
  std::map<std::string, std::string> env;

  std::string contentLength = req.getHeader("Content-Length");
  if (!contentLength.empty()) {
    env["CONTENT_LENGTH"] = contentLength;
  }
  std::string contentType = req.getHeader("Content-Type");
  if (!contentType.empty()) {
    env["CONTENT_TYPE"] = contentType;
  }

  env["GATEWAY_INTERFACE"] = "CGI/1.1";

  env["PATH_INFO"] = req.getPath();
  env["PATH_TRANSLATED"] = realPath;

  env["QUERY_STRING"] = req.getQuery();

  env["REMOTE_ADDR"] = client.getIp();

  HttpMethod method = req.getMethod();
  switch (method) {
    case GET:
      env["REQUEST_METHOD"] = "GET";
      break;
    case POST:
      env["REQUEST_METHOD"] = "POST";
      break;
    case DELETE:
      env["REQUEST_METHOD"] = "DELETE";
      break;
    default:
      env["REQUEST_METHOD"] = "UNKNOWN";
      break;
  }

  env["SCRIPT_NAME"] = req.getPath();
  env["SCRIPT_FILENAME"] = realPath;

  std::string serverName = req.getHeader("Host");
  if (serverName.empty()) {
    serverName = client.getIp();
  } else {
    size_t colonPos = serverName.find(":");
    if (colonPos != std::string::npos)
      serverName = serverName.substr(0, colonPos);
  }
  env["SERVER_NAME"] = serverName;
  env["SERVER_PORT"] = toString(client.getListenPort());
  env["SERVER_PROTOCOL"] = "HTTP/1.1";
  env["SERVER_SOFTWARE"] = "webserv/1.0";

  const std::map<std::string, std::string>& headers = req.getHeaders();
  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    std::string key = toEnvKey(it->first);
    if (key == "CONTENT_LENGTH" || key == "CONTENT_TYPE")
      continue;
    env["HTTP_" + key] = it->second;
  }

  char** envp = NULL;
  try {
    envp = new char*[env.size() + 1];
    for (size_t i = 0; i <= env.size(); ++i)
      envp[i] = NULL;
    size_t k = 0;
    for (std::map<std::string, std::string>::const_iterator it = env.begin();
         it != env.end(); ++it, ++k) {
      std::string s = it->first + "=" + it->second;
      envp[k] = new char[s.size() + 1];
      std::copy(s.begin(), s.end(), envp[k]);
      envp[k][s.size()] = '\0';
    }
  } catch (const std::bad_alloc& e) {
    std::cerr << "[Error] createCgiEnv: memory allocation failed: " << e.what()
              << std::endl;
    if (envp) {
      for (size_t i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
      delete[] envp;
    }
    return NULL;
  }
  return envp;
}

void freeCgiEnv(char** envp) {
  if (!envp)
    return;
  for (size_t i = 0; envp[i] != NULL; ++i) {
    delete[] envp[i];
  }
  delete[] envp;
}

bool setNonBlocking(int fd) {
  if (fd < 0)
    return false;

  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return false;

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return false;
  return true;
}

}  // namespace

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
  req.clear();
  res.clear();
  if (_epoll && _context) {
    _epoll->mod(_fd, _context, EPOLLIN);
  }
}

void Client::readyToCgiWrite() {
  _state = WAITING_CGI_INPUT;
  if (_epoll && _context && _cgi_stdin_fd != -1) {
    _epoll->add(_cgi_stdin_fd, _context, EPOLLOUT);
  }
}

void Client::readyToCgiRead() {
  _state = READING_CGI_OUTPUT;

  if (_cgi_stdin_fd != -1) {
    if (_epoll)
      _epoll->del(_cgi_stdin_fd);
    close(_cgi_stdin_fd);
    _cgi_stdin_fd = -1;
  }

  if (_epoll && _context && _cgi_stdout_fd != -1) {
    _epoll->add(_cgi_stdout_fd, _context, EPOLLIN);
  }
}

int Client::startCgi(const std::string& scriptPath,
                     const std::string& execPath) {
  int pipe_in[2];
  int pipe_out[2];

  if (pipe(pipe_in) < 0) {
    std::cerr << "[Error] pipe creation failed: " << strerror(errno)
              << std::endl;
    return 500;  // internal server error
  }
  if (pipe(pipe_out) < 0) {
    close(pipe_in[0]);
    close(pipe_in[1]);
    std::cerr << "[Error] pipe creation failed: " << strerror(errno)
              << std::endl;
    return 500;  // internal server error
  }

  _cgi_pid = fork();
  if (_cgi_pid < 0) {
    std::cerr << "[Error] fork failed: " << strerror(errno) << std::endl;
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return 500;  // internal server error
  }

  if (_cgi_pid == 0) {
    close(_fd);
    if (dup2(pipe_in[0], STDIN_FILENO) < 0) {
      std::cerr << "[Error] dup2 stdin failed: " << strerror(errno)
                << std::endl;
      close(pipe_in[0]);
      close(pipe_in[1]);
      close(pipe_out[0]);
      close(pipe_out[1]);
      exit(1);
    }
    if (dup2(pipe_out[1], STDOUT_FILENO) < 0) {
      std::cerr << "[Error] dup2 stdout failed: " << strerror(errno)
                << std::endl;
      close(pipe_in[0]);
      close(pipe_in[1]);
      close(pipe_out[0]);
      close(pipe_out[1]);
      exit(1);
    }
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);

    char** env = createCgiEnv(*this, scriptPath);
    if (!env) {
      std::cerr << "[Error] Failed to create CGI environment" << std::endl;
      exit(1);
    }

    char* argv[3];
    if (!execPath.empty()) {
      argv[0] = const_cast<char*>(execPath.c_str());
      argv[1] = const_cast<char*>(scriptPath.c_str());
      argv[2] = NULL;
      execve(execPath.c_str(), argv, env);
    } else {
      argv[0] = const_cast<char*>(scriptPath.c_str());
      argv[1] = NULL;
      execve(scriptPath.c_str(), argv, env);
    }

    std::cerr << "[Error] execve failed: " << strerror(errno) << std::endl;
    freeCgiEnv(env);
    exit(1);
  }

  close(pipe_in[0]);
  close(pipe_out[1]);

  _cgi_stdin_fd = pipe_in[1];
  _cgi_stdout_fd = pipe_out[0];

  if (!setNonBlocking(_cgi_stdin_fd) || !setNonBlocking(_cgi_stdout_fd)) {
    std::cerr << "[Error] setNonBlocking failed: " << strerror(errno)
              << std::endl;
    _cleanupCgi();
    return 500;  // internal server error
  }

  _cgi_stdin_offset = 0;
  _cgi_output.clear();

  if (req.getMethod() == POST) {
    readyToCgiWrite();
  } else {
    close(_cgi_stdin_fd);
    _cgi_stdin_fd = -1;
    readyToCgiRead();
  }
  return (0);
}

void Client::finishCgi() {
  if (_epoll && _context) {
    _epoll->mod(_fd, _context, EPOLLOUT);
  }

  res.parseCgiResponse(_cgi_output);

  _cleanupCgi();
  readyToWrite();
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

void Client::setCgiPid(pid_t pid) {
  _cgi_pid = pid;
}

void Client::setCgiStdinFd(int fd) {
  _cgi_stdin_fd = fd;
}

void Client::setCgiStdoutFd(int fd) {
  _cgi_stdout_fd = fd;
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
  _cleanupCgi();
  _state = WAIT_REQUEST;
  updateTimestamp();
}

// ========================================
// プライベートヘルパー
// ========================================

void Client::_cleanupCgi() {
  if (_cgi_stdout_fd != -1) {
    if (_epoll)
      _epoll->del(_cgi_stdout_fd);
    close(_cgi_stdout_fd);
    _cgi_stdout_fd = -1;
  }
  if (_cgi_stdin_fd != -1) {
    if (_epoll)
      _epoll->del(_cgi_stdin_fd);
    close(_cgi_stdin_fd);
    _cgi_stdin_fd = -1;
  }
  if (_cgi_pid > 0) {
    waitpid(_cgi_pid, NULL, WNOHANG);
    _cgi_pid = -1;
  }
  _cgi_output.clear();
  _cgi_stdin_offset = 0;
}
