#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include "Client.hpp"
#include "Config.hpp"

/*
 * RequestHandler Class
 * 責務:
 * 1. リクエストの解析結果(HttpRequest)から適切な処理を決定する
 * 2. 設定(Config)に基づき、パスの解決や権限チェックを行う
 * 3. 処理結果を HttpResponse に書き込む
 * 4. Client の状態遷移メソッドを呼び出す (epoll 操作は Client 内部で行われる)
 *
 * 注意:
 * - RequestHandler は EpollUtils を直接操作しない
 * - 状態遷移は client->readyToWrite() や client->startCgi() を呼ぶ
 */
class RequestHandler {
 public:
  RequestHandler(const MainConfig& config);
  ~RequestHandler();

  // メインループから呼ばれる唯一のエントリーポイント
  void handle(Client* client);

 private:
  const MainConfig& _config;

  // --- Core Logic Helpers ---

  // Hostヘッダーを見て、適切なServerConfig (Virtual Host) を特定する
  const ServerConfig* _findServerConfig(const Client* client);

  // URIを見て、適切なLocationブロックを特定する (最長一致など)
  const LocationConfig* _findLocationConfig(const HttpRequest& req,
                                            const ServerConfig& serverConfig);

  // URL を実際のファイルパスに変換
  std::string _resolvePath(const std::string& uri,
                           const ServerConfig& serverConfig,
                           const LocationConfig* location);

  // --- Method Handlers ---
  // 内部で client->readyToWrite() を呼んで状態遷移する

  int _handleGet(Client* client, const std::string& realPath,
                 const LocationConfig* location);
  int _handlePost(Client* client, const std::string& realPath,
                  const LocationConfig* location);
  int _handleDelete(Client* client, const std::string& realPath,
                    const LocationConfig* location);

  // --- Specific Features ---

  // CGIの実行処理 (内部で client->startCgi() を呼ぶ)
  void _handleCgi(Client* client, const std::string& scriptPath,
                  const LocationConfig* location);

  // ディレクトリリスティング (AutoIndex) の生成
  void _generateAutoIndex(Client* client, const std::string& dirPath);

  // HTTPリダイレクト処理 (301, 302など)
  void _handleRedirection(Client* client, const LocationConfig* location);

  // エラーレスポンスの生成 (内部で client->readyToWrite() を呼ぶ)
  bool _handleError(Client* client, int statusCode);

  // --- Utilities ---
  bool _isCgiRequest(const std::string& path, const LocationConfig* location);
  bool _isDirectory(const std::string& path);
  bool _isFileExist(const std::string& path);
  bool _checkPermission(const std::string& path, const std::string& mode);

  // Orthodox Canonical Form (コピー禁止)
  RequestHandler(const RequestHandler&);
  RequestHandler& operator=(const RequestHandler&);
};

#endif