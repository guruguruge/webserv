#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include "Client.hpp"
#include "Config.hpp"

/*
 * RequestHandler Class
 * 責務:
 * 1. リクエストの解析結果(HttpRequest)から適切な処理を決定する
 * 2. 設定(Config)に基づき、パスの解決や権限チェックを行う
 * 3. 処理結果を HttpResponse に書き込む
 * 4. CGIの実行やエラーハンドリングのトリガーを引く
 */
class RequestHandler {
 public:
  // コンストラクタで全体設定を受け取る（参照で保持し、コピーコストを防ぐ）
  RequestHandler(const MainConfig& config);
  ~RequestHandler();

  // メインループから呼ばれる唯一のエントリーポイント
  // 内部でステートを変更したり、レスポンスを構築したりする
  void handle(Client* client);

 private:
  const MainConfig& _config;

  // --- Core Logic Helpers ---

  // Hostヘッダーを見て、適切なServerConfig (Virtual Host) を特定する
  const ServerConfig* _findServerConfig(const Client* client);

  // URIを見て、適切なLocationブロックを特定する (最長一致など)
  const LocationConfig* _findLocationConfig(const HttpRequest& req,
                                            const ServerConfig& serverConfig);

  // URL (e.g. /index.html) を 実際のファイルパス (e.g. /var/www/html/index.html) に変換
  std::string _resolvePath(const std::string& uri,
                           const LocationConfig* location);

  // --- Method Handlers ---
  // 具体的な処理。必要に応じてClientの状態を変更する

  void _handleGet(Client* client, const std::string& realPath,
                  const LocationConfig* location);
  void _handlePost(Client* client, const std::string& realPath,
                   const LocationConfig* location);
  void _handleDelete(Client* client, const std::string& realPath,
                     const LocationConfig* location);

  // --- Specific Features ---

  // CGIの実行処理 (Pipe作成、fork, execve, Client状態のCGI待ちへの変更)
  void _handleCgi(Client* client, const std::string& scriptPath,
                  const LocationConfig* location);

  // ディレクトリリスティング (AutoIndex) の生成
  void _generateAutoIndex(Client* client, const std::string& dirPath);

  // HTTPリダイレクト処理 (301, 302など)
  void _handleRedirection(Client* client, const LocationConfig* location);

  // エラーレスポンスの生成 (カスタムエラーページ or buildErrorHtml)
  void _handleError(Client* client, int statusCode);

  // --- Utilities ---
  bool _isCgiRequest(const std::string& path, const LocationConfig* location);
  bool _isDirectory(const std::string& path);
  bool _isFileExist(const std::string& path);
  bool _checkPermission(const std::string& path, const std::string& mode);

  // 禁止コピーコンストラクタ (C++98 style)
  RequestHandler(const RequestHandler&);
  RequestHandler& operator=(const RequestHandler&);
};

#endif