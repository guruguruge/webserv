#include "CgiHandler.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include "ErrorPageManager.hpp"

bool CgiHandler::startCgi(ClientConnection& client, const HttpRequest& req,
                          const ServerConfig& server,
                          const LocationConfig& location) {
  std::cout << "🔧 startCgi called" << std::endl;

  // スクリプトパスを解決
  std::string scriptPath = resolveScriptPath(req, location);

  std::cout << "📁 Script path: " << scriptPath << std::endl;

  // スクリプトファイルの存在確認
  struct stat st;
  if (stat(scriptPath.c_str(), &st) != 0) {
    std::cerr << "❌ CGI script not found: " << scriptPath << std::endl;
    HttpResponse response = ErrorPageManager::makeErrorResponse(
        404, &server, "CGI script not found");

    // Connectionヘッダーを追加
    if (client.isKeepAlive()) {
      response.setHeader("Connection", "keep-alive");
    } else {
      response.setHeader("Connection", "close");
    }

    client.getSendBuffer() = response.serialize();
    client.setState(ClientConnection::WRITING);
    return false;
  }

  // パイプを作成 (stdin用, stdout用)
  int pipeStdin[2];
  int pipeStdout[2];

  if (pipe(pipeStdin) < 0 || pipe(pipeStdout) < 0) {
    std::cerr << "❌ Failed to create pipes for CGI" << std::endl;
    HttpResponse response = ErrorPageManager::makeErrorResponse(
        500, &server, "Failed to create pipes");

    // Connectionヘッダーを追加
    if (client.isKeepAlive()) {
      response.setHeader("Connection", "keep-alive");
    } else {
      response.setHeader("Connection", "close");
    }

    client.getSendBuffer() = response.serialize();
    client.setState(ClientConnection::WRITING);
    return false;
  }

  // fork
  pid_t pid = fork();

  if (pid < 0) {
    // fork失敗
    std::cerr << "❌ Failed to fork for CGI" << std::endl;
    close(pipeStdin[0]);
    close(pipeStdin[1]);
    close(pipeStdout[0]);
    close(pipeStdout[1]);
    HttpResponse response =
        ErrorPageManager::makeErrorResponse(500, &server, "Failed to fork");

    // Connectionヘッダーを追加
    if (client.isKeepAlive()) {
      response.setHeader("Connection", "keep-alive");
    } else {
      response.setHeader("Connection", "close");
    }

    client.getSendBuffer() = response.serialize();
    client.setState(ClientConnection::WRITING);
    return false;
  }

  if (pid == 0) {
    // 子プロセス

    // stdin を pipeStdin[0] に接続
    dup2(pipeStdin[0], STDIN_FILENO);
    close(pipeStdin[0]);
    close(pipeStdin[1]);

    // stdout を pipeStdout[1] に接続
    dup2(pipeStdout[1], STDOUT_FILENO);
    close(pipeStdout[0]);
    close(pipeStdout[1]);

    // 環境変数とargvを構築
    char** envp = buildEnvp(req, server, location, scriptPath);
    char** argv = buildArgv(location, scriptPath);

    // CGIインタープリタを実行
    execve(location.cgiPath.c_str(), argv, envp);

    // execve失敗
    std::cerr << "❌ execve failed for CGI: " << location.cgiPath << std::endl;
    exit(1);
  }

  // 親プロセス

  // 使わない側のパイプを閉じる
  close(pipeStdin[0]);
  close(pipeStdout[1]);

  // POSTの場合、リクエストボディをCGIのstdinに書き込む
  if (req.method == "POST" && !req.body.empty()) {
    std::string bodyStr(req.body.begin(), req.body.end());
    write(pipeStdin[1], bodyStr.c_str(), bodyStr.size());
  }
  close(pipeStdin[1]);

  // stdout を非ブロッキングに設定
  int flags = fcntl(pipeStdout[0], F_GETFL, 0);
  fcntl(pipeStdout[0], F_SETFL, flags | O_NONBLOCK);

  // ClientConnection に CGI stdout fd を設定
  client.setCgiStdoutFd(pipeStdout[0]);
  client.setState(ClientConnection::CGI_WAIT);

  std::cout << "🔄 CGI started: " << location.cgiPath << " " << scriptPath
            << " (pid: " << pid << ", stdout fd: " << pipeStdout[0] << ")"
            << std::endl;

  return true;  // CGI開始成功
}

bool CgiHandler::onCgiStdoutReadable(ClientConnection& client) {
  int fd = client.getCgiStdoutFd();
  char buffer[4096];

  ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

  if (bytesRead > 0) {
    // データを読み取った
    client.appendCgiOutput(std::string(buffer, bytesRead));
    std::cout << "📖 Read " << bytesRead << " bytes from CGI (fd: " << fd << ")"
              << std::endl;
    return false;  // まだ継続中
  } else if (bytesRead == 0) {
    // EOF - CGI完了
    std::cout << "✅ CGI finished (fd: " << fd << ")" << std::endl;
    close(fd);
    client.setCgiStdoutFd(-1);

    // CGI出力をHTTPレスポンスに変換
    HttpResponse response = parseCgiOutput(client.getCgiOutputBuffer());

    // Connectionヘッダーを追加
    if (client.isKeepAlive()) {
      response.setHeader("Connection", "keep-alive");
    } else {
      response.setHeader("Connection", "close");
    }

    client.getSendBuffer() = response.serialize();
    client.setState(ClientConnection::WRITING);

    // CGI出力バッファをクリア
    client.getCgiOutputBuffer().clear();

    return true;  // CGI完了
  } else {
    // エラー (EAGAIN/EWOULDBLOCKは正常)
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;  // まだ継続中
    }

    std::cerr << "❌ Error reading from CGI: " << strerror(errno) << std::endl;
    close(fd);
    client.setCgiStdoutFd(-1);

    HttpResponse response;
    response.setStatusCode(502);
    response.setReasonPhrase("Bad Gateway");
    response.setBody("CGI read error");

    // Connectionヘッダーを追加
    if (client.isKeepAlive()) {
      response.setHeader("Connection", "keep-alive");
    } else {
      response.setHeader("Connection", "close");
    }

    client.getSendBuffer() = response.serialize();
    client.setState(ClientConnection::WRITING);

    return true;  // エラーで終了
  }
}

char** CgiHandler::buildEnvp(const HttpRequest& req, const ServerConfig& server,
                             const LocationConfig& location,
                             const std::string& scriptPath) {
  (void)location;  // 現在未使用だが、将来的に使用する可能性がある

  std::vector<std::string> env;

  // REQUEST_METHOD
  env.push_back("REQUEST_METHOD=" + req.method);

  // SCRIPT_FILENAME
  env.push_back("SCRIPT_FILENAME=" + scriptPath);

  // QUERY_STRING
  env.push_back("QUERY_STRING=" + req.query);

  // CONTENT_LENGTH
  std::ostringstream oss;
  oss << req.body.size();
  env.push_back("CONTENT_LENGTH=" + oss.str());

  // CONTENT_TYPE
  std::map<std::string, std::string>::const_iterator it =
      req.headers.find("content-type");
  if (it != req.headers.end()) {
    env.push_back("CONTENT_TYPE=" + it->second);
  } else {
    env.push_back("CONTENT_TYPE=");
  }

  // SERVER_PROTOCOL
  env.push_back("SERVER_PROTOCOL=" + req.httpVersion);

  // SERVER_NAME
  env.push_back("SERVER_NAME=" + server.serverName);

  // PATH_INFO (リクエストパス)
  env.push_back("PATH_INFO=" + req.path);

  // SCRIPT_NAME
  env.push_back("SCRIPT_NAME=" + req.path);

  // HTTP_* ヘッダ (必要に応じて)
  for (std::map<std::string, std::string>::const_iterator it =
           req.headers.begin();
       it != req.headers.end(); ++it) {
    std::string key = "HTTP_";
    for (size_t i = 0; i < it->first.size(); ++i) {
      char c = it->first[i];
      if (c == '-') {
        key += '_';
      } else if (c >= 'a' && c <= 'z') {
        key += (c - 'a' + 'A');
      } else {
        key += c;
      }
    }
    env.push_back(key + "=" + it->second);
  }

  // envp配列を作成
  char** envp = new char*[env.size() + 1];
  for (size_t i = 0; i < env.size(); ++i) {
    envp[i] = new char[env[i].size() + 1];
    std::strcpy(envp[i], env[i].c_str());
  }
  envp[env.size()] = NULL;

  return envp;
}

char** CgiHandler::buildArgv(const LocationConfig& location,
                             const std::string& scriptPath) {
  char** argv = new char*[3];

  // argv[0] = CGIインタープリタ
  argv[0] = new char[location.cgiPath.size() + 1];
  std::strcpy(argv[0], location.cgiPath.c_str());

  // argv[1] = スクリプトパス
  argv[1] = new char[scriptPath.size() + 1];
  std::strcpy(argv[1], scriptPath.c_str());

  argv[2] = NULL;

  return argv;
}

void CgiHandler::freeEnvp(char** envp) {
  if (!envp)
    return;
  for (int i = 0; envp[i] != NULL; ++i) {
    delete[] envp[i];
  }
  delete[] envp;
}

void CgiHandler::freeArgv(char** argv) {
  if (!argv)
    return;
  for (int i = 0; argv[i] != NULL; ++i) {
    delete[] argv[i];
  }
  delete[] argv;
}

HttpResponse CgiHandler::parseCgiOutput(const std::string& cgiOutput) {
  HttpResponse response;

  // ヘッダとボディを分割
  size_t headerEnd = cgiOutput.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    headerEnd = cgiOutput.find("\n\n");
    if (headerEnd == std::string::npos) {
      // ヘッダなし - 全てボディ
      response.setStatusCode(200);
      response.setReasonPhrase("OK");
      response.setBody(cgiOutput);
      response.setHeader("Content-Type", "text/html");
      return response;
    }
    headerEnd += 2;
  } else {
    headerEnd += 4;
  }

  std::string headers = cgiOutput.substr(0, headerEnd);
  std::string body = cgiOutput.substr(headerEnd);

  // デフォルトステータス
  int statusCode = 200;
  std::string reasonPhrase = "OK";

  // ヘッダをパース
  std::istringstream iss(headers);
  std::string line;

  while (std::getline(iss, line)) {
    if (line.empty() || line == "\r") {
      break;
    }

    // 改行を削除
    if (!line.empty() && line[line.size() - 1] == '\r') {
      line = line.substr(0, line.size() - 1);
    }

    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);

    // 先頭の空白を削除
    while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
      value = value.substr(1);
    }

    // Status ヘッダをチェック
    if (key == "Status") {
      // "200 OK" のような形式
      size_t spacePos = value.find(' ');
      if (spacePos != std::string::npos) {
        statusCode = atoi(value.substr(0, spacePos).c_str());
        reasonPhrase = value.substr(spacePos + 1);
      } else {
        statusCode = atoi(value.c_str());
      }
    } else {
      response.setHeader(key, value);
    }
  }

  response.setStatusCode(statusCode);
  response.setReasonPhrase(reasonPhrase);
  response.setBody(body);

  // Content-Typeがない場合はデフォルト
  if (headers.find("Content-Type:") == std::string::npos &&
      headers.find("content-type:") == std::string::npos) {
    response.setHeader("Content-Type", "text/html");
  }

  return response;
}

std::string CgiHandler::resolveScriptPath(const HttpRequest& req,
                                          const LocationConfig& location) {
  // リクエストパスから先頭のスラッシュを除去
  std::string relativePath = req.path;
  if (!relativePath.empty() && relativePath[0] == '/') {
    relativePath = relativePath.substr(1);
  }

  // root + relativePath
  std::string scriptPath = location.root;
  if (!scriptPath.empty() && scriptPath[scriptPath.size() - 1] != '/') {
    scriptPath += '/';
  }
  scriptPath += relativePath;

  return scriptPath;
}
