#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map>
#include <string>
#include <vector>
#include "Defines.hpp"

// Locationブロック (/images/ などの設定)
struct LocationConfig {
  std::string path;                       // ex: "/tmp"
  std::string root;                       // ex: "/var/www/html"
  std::string index;                      // ex: "index.html"
  std::vector<HttpMethod> allow_methods;  // GET, POST...
  std::string cgi_extension;              // ex: ".php"
  std::string upload_path;                // ex: "/uploads"
  bool autoindex;
  std::string redirect_url;  // 301用
};

// Serverブロック (ポート、サーバー名ごとの設定)
struct ServerConfig {
  int listen_port;
  std::string host;                        // ex: "127.0.0.1"
  std::vector<std::string> server_names;   // ex: "example.com"
  std::map<int, std::string> error_pages;  // 404 -> "/404.html"
  size_t client_max_body_size;
  std::vector<LocationConfig> locations;

  // パスに最も長くマッチするLocationを返す
  const LocationConfig* getLocation(const std::string& path) const;
};

// 全体の設定コンテナ
class MainConfig {
 public:
  std::vector<ServerConfig> servers;

  MainConfig();  // コンストラクタでConfigパーサーを呼ぶか、別途loadメソッドを作る
  ~MainConfig();

  bool load(const std::string& file_path);

  // Hostヘッダとポート番号から最適なServerConfigを特定する
  const ServerConfig* getServer(const std::string& host, int port) const;
};

#endif
