#include "Router.hpp"
#include <algorithm>
#include <cstdlib>

const ServerConfig* Router::findServer(const Config& config,
                                       const HttpRequest& request) {
  if (config.servers.empty()) {
    return NULL;
  }

  // Hostヘッダーからポート番号を取得
  int requestPort = 80;  // デフォルト
  if (!request.port.empty()) {
    requestPort = std::atoi(request.port.c_str());
  }

  // 1. Hostヘッダーのホスト名とポートが一致するサーバーを探す
  for (size_t i = 0; i < config.servers.size(); ++i) {
    const ServerConfig& server = config.servers[i];

    // ポートが一致するかチェック
    if (!isPortMatching(server, requestPort)) {
      continue;
    }

    // server_nameが一致するかチェック
    if (!server.serverName.empty() && server.serverName == request.host) {
      return &server;
    }
  }

  // 2. ポートのみが一致するサーバーを探す
  for (size_t i = 0; i < config.servers.size(); ++i) {
    const ServerConfig& server = config.servers[i];
    if (isPortMatching(server, requestPort)) {
      return &server;
    }
  }

  // 3. どれにも一致しない場合は最初のサーバーを返す
  return &config.servers[0];
}

const LocationConfig* Router::findLocation(const ServerConfig& serverConfig,
                                           const std::string& path) {
  const LocationConfig* bestMatch = NULL;
  size_t bestMatchLength = 0;

  // 最長プレフィックスマッチングを行う
  for (size_t i = 0; i < serverConfig.locations.size(); ++i) {
    const LocationConfig& location = serverConfig.locations[i];

    // パスがロケーションパスで始まっているかチェック
    if (path.find(location.path) == 0) {
      size_t matchLength = location.path.size();

      // より長いマッチを優先
      if (matchLength > bestMatchLength) {
        bestMatch = &location;
        bestMatchLength = matchLength;
      }
    }
  }

  return bestMatch;
}

bool Router::isPortMatching(const ServerConfig& serverConfig, int port) {
  for (size_t i = 0; i < serverConfig.listen.size(); ++i) {
    const std::string& listenStr = serverConfig.listen[i];

    // listenは "host:port", ":port", "port" のいずれかの形式
    // ポート番号を抽出
    size_t colonPos = listenStr.find(':');
    int listenPort;

    if (colonPos != std::string::npos) {
      // "host:port" or ":port" 形式
      std::string portStr = listenStr.substr(colonPos + 1);
      listenPort = std::atoi(portStr.c_str());
    } else {
      // "port" 形式
      listenPort = std::atoi(listenStr.c_str());
    }

    if (listenPort == port) {
      return true;
    }
  }
  return false;
}
