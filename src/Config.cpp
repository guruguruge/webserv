#include "Config.hpp"

// ============================================================================
// LocationConfig
// ============================================================================

LocationConfig::LocationConfig()
    : path("/"),
      root(""),
      alias(""),
      index("index.html"),
      cgi_extension(""),
      cgi_path(""),
      upload_path(""),
      autoindex(false),
      return_redirect(std::make_pair(0, "")) {
  allow_methods.push_back(GET);
}

// ============================================================================
// ServerConfig
// ============================================================================

ServerConfig::ServerConfig()
    : listen_port(80),
      host("0.0.0.0"),
      client_max_body_size(DEFAULT_CLIENT_MAX_BODY_SIZE) {}

// 最長プレフィックスマッチでLocationConfigを返す
const LocationConfig* ServerConfig::getLocation(const std::string& path) const {
  const LocationConfig* best_match = NULL;
  size_t best_match_len = 0;

  for (size_t i = 0; i < locations.size(); ++i) {
    const std::string& loc_path = locations[i].path;
    bool is_match = false;

    if (loc_path == "/") {
      // ルートは全てにマッチ
      is_match = true;
    } else if (path == loc_path) {
      // 完全一致
      is_match = true;
    } else if (path.length() > loc_path.length() &&
               path.compare(0, loc_path.length(), loc_path) == 0 &&
               path[loc_path.length()] == '/') {
      is_match = true;
    }

    if (is_match && loc_path.length() > best_match_len) {
      best_match = &locations[i];
      best_match_len = loc_path.length();
    }
  }

  return best_match;
}

// ============================================================================
// MainConfig
// ============================================================================

MainConfig::MainConfig() {}

MainConfig::~MainConfig() {}

bool MainConfig::load(const std::string& file_path) {
  // TODO: ConfigParser実装後に完成させる
  (void)file_path;
  return false;
}

// Hostヘッダを正規化する（ポート除去、小文字化、末尾ドット除去）
static std::string normalizeHost(const std::string& host) {
  std::string normalized = host;

  // 末尾の '.' を除去 (FQDN対応)
  if (!normalized.empty() && normalized[normalized.length() - 1] == '.') {
    normalized.erase(normalized.length() - 1);
  }

  // ポート番号を除去
  // "example.com:8080" -> "example.com"
  // "[::1]:8080" -> "[::1]"
  size_t bracket_pos = normalized.find(']');
  size_t colon_pos;
  if (bracket_pos != std::string::npos) {
    // IPv6: ']' より後の ':' を探す
    colon_pos = normalized.find(':', bracket_pos);
  } else {
    // IPv4 or hostname: 最後の ':' を探す
    colon_pos = normalized.rfind(':');
  }
  if (colon_pos != std::string::npos) {
    normalized = normalized.substr(0, colon_pos);
  }

  // 小文字化 (DNS名は大文字小文字を区別しない)
  for (size_t i = 0; i < normalized.length(); ++i) {
    if (normalized[i] >= 'A' && normalized[i] <= 'Z') {
      normalized[i] = normalized[i] + ('a' - 'A');
    }
  }

  return normalized;
}

// Hostヘッダとポート番号から最適なServerConfigを返す
const ServerConfig* MainConfig::getServer(const std::string& host,
                                          int port) const {
  // serversが空の場合はNULLを返す
  if (servers.empty()) {
    return NULL;
  }

  const std::string normalized_host = normalizeHost(host);
  const ServerConfig* default_server = NULL;

  for (size_t i = 0; i < servers.size(); ++i) {
    const ServerConfig& server = servers[i];

    // ポート番号が一致するか確認
    if (server.listen_port != port) {
      continue;
    }

    // 同じポートの最初のサーバーをデフォルトとして記録
    if (default_server == NULL) {
      default_server = &server;
    }

    // server_namesをチェック（正規化して比較）
    for (size_t j = 0; j < server.server_names.size(); ++j) {
      std::string normalized_server_name =
          normalizeHost(server.server_names[j]);
      if (normalized_server_name == normalized_host) {
        return &server;  // マッチ
      }
    }
  }

  // server_nameがマッチしなかった場合、同じポートのデフォルトサーバーを返す
  // ポートもマッチしなかった場合は最初のサーバーをフォールバックとして返す
  if (default_server == NULL) {
    default_server = &servers[0];
  }

  return default_server;
}
