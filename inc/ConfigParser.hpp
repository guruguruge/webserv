#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <vector>
#include "Config.hpp"

/**
 * @brief nginx風設定ファイルをパースするクラス
 *
 * NGINX形式の設定ファイルを読み込み、MainConfigオブジェクトに変換する。
 * トークナイザと再帰下降パーサを使用。
 *
 * サポートするディレクティブ:
 * - server { }
 * - listen
 * - server_name
 * - root
 * - error_page
 * - client_max_body_size
 * - location { }
 * - index
 * - autoindex
 * - allowed_methods
 * - upload_path
 * - cgi_extension
 * - cgi_path
 * - return (リダイレクト)
 */
class ConfigParser {
 public:
  /**
   * @brief コンストラクタ
   * @param file_path 設定ファイルのパス
   */
  explicit ConfigParser(const std::string& file_path);

  /**
   * @brief デストラクタ
   */
  ~ConfigParser();

  /**
   * @brief 設定ファイルをパースしてMainConfigに格納
   *
   * @param config パース結果を格納するMainConfig
   * @throw std::runtime_error パースエラー時
   */
  void parse(MainConfig& config);

 private:
  std::string file_path_;            ///< 設定ファイルパス
  std::vector<std::string> tokens_;  ///< トークンリスト
  std::vector<int> token_lines_;     ///< 各トークンの行番号
  size_t current_index_;             ///< 現在のトークン位置
  int last_line_;                    ///< 最後に消費したトークンの行番号

  // ============================================================================
  // トークナイザ
  // ============================================================================

  /**
   * @brief ファイルを読み込んでトークンに分割
   * @throw std::runtime_error ファイル読み込みエラー時
   */
  void tokenize();

  /**
   * @brief 1文字がトークン区切り文字かどうか判定
   * @param c 判定する文字
   * @return 区切り文字なら true
   */
  bool isDelimiter(char c) const;

  // ============================================================================
  // トークン操作
  // ============================================================================

  /**
   * @brief 次のトークンを取得して位置を進める
   * @return 次のトークン
   * @throw std::runtime_error トークンがない場合
   */
  std::string nextToken();

  /**
   * @brief 現在のトークンを取得（位置は進めない）
   * @return 現在のトークン、なければ空文字列
   */
  std::string peekToken() const;

  /**
   * @brief まだトークンが残っているか確認
   * @return 残っていれば true
   */
  bool hasMoreTokens() const;

  /**
   * @brief 期待するトークンを消費、一致しなければエラー
   * @param expected 期待するトークン
   * @throw std::runtime_error 一致しない場合
   */
  void expectToken(const std::string& expected);

  /**
   * @brief セミコロンを消費
   * @throw std::runtime_error セミコロンがない場合
   */
  void skipSemicolon();

  // ============================================================================
  // パーサ（ブロック）
  // ============================================================================

  /**
   * @brief serverブロックをパース
   * @param config パース結果を格納するMainConfig
   */
  void parseServerBlock(MainConfig& config);

  /**
   * @brief locationブロックをパース
   * @param server パース結果を格納するServerConfig
   */
  void parseLocationBlock(ServerConfig& server);

  // ============================================================================
  // パーサ（server ディレクティブ）
  // ============================================================================

  /**
   * @brief listenディレクティブをパース
   * @param server パース結果を格納するServerConfig
   */
  void parseListenDirective(ServerConfig& server);

  /**
   * @brief server_nameディレクティブをパース
   * @param server パース結果を格納するServerConfig
   */
  void parseServerNameDirective(ServerConfig& server);

  /**
   * @brief error_pageディレクティブをパース
   * @param server パース結果を格納するServerConfig
   */
  void parseErrorPageDirective(ServerConfig& server);

  /**
   * @brief client_max_body_sizeディレクティブをパース
   * @param server パース結果を格納するServerConfig
   */
  void parseClientMaxBodySizeDirective(ServerConfig& server);

  /**
   * @brief server直下のrootディレクティブをパース
   * @param server パース結果を格納するServerConfig
   */
  void parseServerRootDirective(ServerConfig& server);

  // ============================================================================
  // パーサ（location ディレクティブ）
  // ============================================================================

  /**
   * @brief rootディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseRootDirective(LocationConfig& location);

  /**
   * @brief aliasディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseAliasDirective(LocationConfig& location);

  /**
   * @brief indexディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseIndexDirective(LocationConfig& location);

  /**
   * @brief autoindexディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseAutoindexDirective(LocationConfig& location);

  /**
   * @brief allowed_methodsディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseAllowedMethodsDirective(LocationConfig& location);

  /**
   * @brief upload_pathディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseUploadPathDirective(LocationConfig& location);

  /**
   * @brief cgi_extensionディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseCgiExtensionDirective(LocationConfig& location);

  /**
   * @brief cgi_pathディレクティブをパース
   * @param location パース結果を格納するLocationConfig
   */
  void parseCgiPathDirective(LocationConfig& location);

  /**
   * @brief returnディレクティブをパース（リダイレクト）
   * @param location パース結果を格納するLocationConfig
   */
  void parseReturnDirective(LocationConfig& location);

  // ============================================================================
  // ユーティリティ
  // ============================================================================

  /**
   * @brief サイズ文字列をバイト数に変換
   *
   * "10M" -> 10485760, "1K" -> 1024, "100" -> 100
   *
   * @param size_str サイズ文字列
   * @return バイト数
   * @throw std::runtime_error 不正な形式の場合
   */
  size_t parseSize(const std::string& size_str) const;

  /**
   * @brief 文字列が数値かどうか判定
   * @param str 判定する文字列
   * @return 数値なら true
   */
  bool isNumber(const std::string& str) const;

  /**
   * @brief エラーメッセージを生成
   * @param message エラー内容
   * @return ファイル名と行番号を含むエラーメッセージ
   */
  std::string makeError(const std::string& message) const;

  // コピー禁止
  ConfigParser(const ConfigParser&);
  ConfigParser& operator=(const ConfigParser&);
};

#endif
