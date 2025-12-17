#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map>
#include <string>
#include <vector>
#include "Defines.hpp"

/**
 * @brief Locationブロックの設定を保持する構造体
 *
 * nginxのlocationディレクティブに相当し、URLパスごとの
 * ルーティング設定を管理する。
 */
struct LocationConfig {
  std::string path;   ///< マッチするURLパス (ex: "/api")
  std::string root;   ///< ドキュメントルート (ex: "/var/www/html")
  std::string alias;  ///< パス置換用エイリアス (ex: "/var/www/static")
  std::string index;  ///< デフォルトインデックスファイル (ex: "index.html")
  std::vector<HttpMethod>
      allow_methods;          ///< 許可するHTTPメソッド (GET, POST, DELETE)
  std::string cgi_extension;  ///< CGI拡張子 (ex: ".py")
  std::string cgi_path;       ///< CGI実行パス (ex: "/usr/bin/python3")
  std::string upload_path;    ///< アップロード先ディレクトリ (ex: "/uploads")
  bool autoindex;             ///< ディレクトリリスティングの有効/無効
  std::pair<int, std::string>
      return_redirect;  ///< リダイレクト設定 (status, URL)

  /**
   * @brief デフォルトコンストラクタ
   *
   * デフォルト値:
   * - path: "/"
   * - index: "index.html"
   * - autoindex: false
   * - allow_methods: [GET]
   */
  LocationConfig();
};

/**
 * @brief Serverブロックの設定を保持する構造体
 *
 * nginxのserverディレクティブに相当し、ポートやサーバー名
 * ごとの設定を管理する。
 */
struct ServerConfig {
  int listen_port;   ///< リッスンポート (デフォルト: 80)
  std::string host;  ///< バインドアドレス (ex: "0.0.0.0", "127.0.0.1")
  std::vector<std::string>
      server_names;  ///< サーバー名リスト (ex: "example.com")
  std::map<int, std::string>
      error_pages;  ///< エラーページマップ (404 -> "/404.html")
  size_t
      client_max_body_size;  ///< クライアントボディ最大サイズ (デフォルト: 1MB)
  std::vector<LocationConfig> locations;  ///< Location設定リスト

  /**
   * @brief デフォルトコンストラクタ
   *
   * デフォルト値:
   * - listen_port: 80
   * - host: "0.0.0.0"
   * - client_max_body_size: DEFAULT_CLIENT_MAX_BODY_SIZE (1MB)
   */
  ServerConfig();

  /**
   * @brief パスに最も長くマッチするLocationを返す
   *
   * 最長プレフィックスマッチアルゴリズムを使用。
   * - "/foo" は "/foo/bar" にマッチするが、"/foobar" にはマッチしない
   * - "/" は全てのパスにマッチする
   *
   * @param path リクエストのURLパス
   * @return マッチしたLocationConfigへのポインタ、マッチなしの場合はNULL
   */
  const LocationConfig* getLocation(const std::string& path) const;
};

/**
 * @brief 全体の設定を管理するクラス
 *
 * 設定ファイルから読み込んだ全てのServerブロックを保持し、
 * リクエストに対して適切なServerConfigを返す機能を提供する。
 */
class MainConfig {
 public:
  std::vector<ServerConfig> servers;  ///< Server設定リスト

  /**
   * @brief デフォルトコンストラクタ
   */
  MainConfig();

  /**
   * @brief デストラクタ
   */
  ~MainConfig();

  /**
   * @brief 設定ファイルをロードする
   *
   * @param file_path 設定ファイルのパス
   * @return 成功時true、失敗時false
   */
  bool load(const std::string& file_path);

  /**
   * @brief Hostヘッダとポート番号から最適なServerConfigを特定する
   *
   * Hostヘッダは以下の正規化を行って比較:
   * - ポート番号の除去 ("example.com:8080" -> "example.com")
   * - 小文字化 ("EXAMPLE.COM" -> "example.com")
   * - 末尾ドット除去 ("example.com." -> "example.com")
   *
   * @param host Hostヘッダの値
   * @param port リクエストを受けたポート番号
   * @return マッチしたServerConfigへのポインタ、serversが空の場合はNULL
   */
  const ServerConfig* getServer(const std::string& host, int port) const;
};

#endif
