#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

// ============================================================================
// 定数定義
// ============================================================================

namespace {
// ポート番号の範囲
const int PORT_MIN = 0;
const int PORT_MAX = 65535;

// HTTPステータスコードの範囲
const int STATUS_CODE_MIN = 100;
const int STATUS_CODE_MAX = 599;

// リダイレクトステータスコードの範囲
const int REDIRECT_CODE_MIN = 300;
const int REDIRECT_CODE_MAX = 399;

// サイズ単位（バイト）
const size_t KILOBYTE = 1024;
const size_t MEGABYTE = 1024 * 1024;
const size_t GIGABYTE = 1024 * 1024 * 1024;
}  // namespace

// ============================================================================
// コンストラクタ・デストラクタ
// ============================================================================

ConfigParser::ConfigParser(const std::string& file_path)
    : file_path_(file_path), current_index_(0), last_line_(1) {}

ConfigParser::~ConfigParser() {}

// ============================================================================
// メインパース関数
// ============================================================================

void ConfigParser::parse(MainConfig& config) {
  tokenize();

  while (hasMoreTokens()) {
    std::string token = peekToken();
    if (token == "server") {
      parseServerBlock(config);
    } else if (token == "#") {
      // コメントはトークナイズ時にスキップされるはずだが念のため
      nextToken();
    } else {
      throw std::runtime_error(makeError("unexpected token: " + token));
    }
  }
}

// ============================================================================
// トークナイザ
// 注意: クォートで囲まれた文字列は非対応。
//       スペースを含むパス（例: root "/var/www/my site";)は使用不可。
// ============================================================================

void ConfigParser::tokenize() {
  // 状態を初期化（複数回呼び出しに対応）
  tokens_.clear();
  token_lines_.clear();
  current_index_ = 0;
  last_line_ = 1;

  std::ifstream file(file_path_.c_str());
  if (!file.is_open()) {
    throw std::runtime_error("failed to open config file: " + file_path_);
  }

  std::string line;
  int line_number = 0;

  while (std::getline(file, line)) {
    ++line_number;

    // コメント除去
    size_t comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }

    // 行ごとにトークナイズ
    std::string token;
    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];

      if (isDelimiter(c)) {
        // 現在のトークンを保存
        if (!token.empty()) {
          tokens_.push_back(token);
          token_lines_.push_back(line_number);
          token.clear();
        }
        // 区切り文字自体もトークンとして保存（空白以外）
        if (c == '{' || c == '}' || c == ';') {
          tokens_.push_back(std::string(1, c));
          token_lines_.push_back(line_number);
        }
      } else {
        token += c;
      }
    }

    // 行末のトークン
    if (!token.empty()) {
      tokens_.push_back(token);
      token_lines_.push_back(line_number);
    }
  }
  file.close();
}

bool ConfigParser::isDelimiter(char c) const {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '{' ||
         c == '}' || c == ';';
}

// ============================================================================
// トークン操作
// ============================================================================

std::string ConfigParser::nextToken() {
  if (current_index_ >= tokens_.size()) {
    throw std::runtime_error(makeError("unexpected end of file"));
  }
  last_line_ = token_lines_[current_index_];
  return tokens_[current_index_++];
}

std::string ConfigParser::peekToken() const {
  if (current_index_ >= tokens_.size()) {
    return "";
  }
  return tokens_[current_index_];
}

bool ConfigParser::hasMoreTokens() const {
  return current_index_ < tokens_.size();
}

void ConfigParser::expectToken(const std::string& expected) {
  std::string token = nextToken();
  if (token != expected) {
    throw std::runtime_error(
        makeError("expected '" + expected + "', got '" + token + "'"));
  }
}

void ConfigParser::skipSemicolon() {
  expectToken(";");
}

// ============================================================================
// パーサ（ブロック）
// ============================================================================

void ConfigParser::parseServerBlock(MainConfig& config) {
  expectToken("server");
  expectToken("{");

  ServerConfig server;
  bool has_listen = false;

  while (hasMoreTokens() && peekToken() != "}") {
    std::string directive = nextToken();

    if (directive == "listen") {
      if (has_listen) {
        throw std::runtime_error(makeError("duplicate 'listen' directive"));
      }
      parseListenDirective(server);
      has_listen = true;
    } else if (directive == "server_name") {
      parseServerNameDirective(server);
    } else if (directive == "root") {
      parseServerRootDirective(server);
    } else if (directive == "error_page") {
      parseErrorPageDirective(server);
    } else if (directive == "client_max_body_size") {
      parseClientMaxBodySizeDirective(server);
    } else if (directive == "location") {
      parseLocationBlock(server);
    } else {
      throw std::runtime_error(
          makeError("unknown server directive: " + directive));
    }
  }

  // listenディレクティブは必須
  if (!has_listen) {
    throw std::runtime_error(
        makeError("'listen' directive is required in server block"));
  }

  expectToken("}");
  config.servers.push_back(server);
}

void ConfigParser::parseLocationBlock(ServerConfig& server) {
  // location パスを取得
  std::string path = nextToken();

  // 重複locationチェック
  for (size_t i = 0; i < server.locations.size(); ++i) {
    if (server.locations[i].path == path) {
      throw std::runtime_error(makeError("duplicate location path: " + path));
    }
  }

  expectToken("{");

  LocationConfig location;
  location.path = path;
  bool has_return = false;

  while (hasMoreTokens() && peekToken() != "}") {
    std::string directive = nextToken();

    if (directive == "root") {
      parseRootDirective(location);
    } else if (directive == "alias") {
      parseAliasDirective(location);
    } else if (directive == "index") {
      parseIndexDirective(location);
    } else if (directive == "autoindex") {
      parseAutoindexDirective(location);
    } else if (directive == "allowed_methods") {
      parseAllowedMethodsDirective(location);
    } else if (directive == "upload_path") {
      parseUploadPathDirective(location);
    } else if (directive == "cgi_extension") {
      parseCgiExtensionDirective(location);
    } else if (directive == "cgi_path") {
      parseCgiPathDirective(location);
    } else if (directive == "return") {
      if (has_return) {
        throw std::runtime_error(makeError("duplicate 'return' directive"));
      }
      parseReturnDirective(location);
      has_return = true;
    } else {
      throw std::runtime_error(
          makeError("unknown location directive: " + directive));
    }
  }

  expectToken("}");
  server.locations.push_back(location);
}

// ============================================================================
// パーサ（server ディレクティブ）
// ============================================================================

void ConfigParser::parseListenDirective(ServerConfig& server) {
  std::string value = nextToken();

  // IPv6は非対応（'['を含む場合はエラー）
  if (value.find('[') != std::string::npos) {
    throw std::runtime_error(
        makeError("IPv6 addresses are not supported: " + value));
  }

  std::string port_str = value;

  // host:port 形式の場合
  size_t colon_pos = value.find(':');
  if (colon_pos != std::string::npos) {
    // コロンが複数ある場合はエラー
    if (value.find(':', colon_pos + 1) != std::string::npos) {
      throw std::runtime_error(
          makeError("invalid listen format (multiple colons): " + value));
    }
    // hostが空の場合はエラー
    if (colon_pos == 0) {
      throw std::runtime_error(
          makeError("invalid listen format (empty host): " + value));
    }
    // portが空の場合はエラー
    if (colon_pos == value.size() - 1) {
      throw std::runtime_error(
          makeError("invalid listen format (empty port): " + value));
    }
    server.host = value.substr(0, colon_pos);
    port_str = value.substr(colon_pos + 1);
  }

  // ポート番号を数値に変換
  std::istringstream iss(port_str);
  int port;
  if (!(iss >> port) || port < PORT_MIN || port > PORT_MAX) {
    throw std::runtime_error(makeError("invalid port number: " + port_str));
  }
  server.listen_port = port;

  skipSemicolon();
}

void ConfigParser::parseServerNameDirective(ServerConfig& server) {
  // セミコロンまで複数のサーバー名を読む
  while (hasMoreTokens() && peekToken() != ";") {
    std::string name = nextToken();
    // 小文字に正規化（DNS名は大文字小文字を区別しない）
    for (size_t i = 0; i < name.length(); ++i) {
      if (name[i] >= 'A' && name[i] <= 'Z') {
        name[i] = name[i] + ('a' - 'A');
      }
    }
    // 末尾ドット除去（FQDN対応）
    if (!name.empty() && name[name.length() - 1] == '.') {
      name.erase(name.length() - 1);
    }
    server.server_names.push_back(name);
  }
  skipSemicolon();
}

void ConfigParser::parseErrorPageDirective(ServerConfig& server) {
  // error_page 404 500 502 /error.html; の形式
  std::vector<int> codes;

  // エラーコードを集める（数字の間）
  while (hasMoreTokens() && peekToken() != ";" && isNumber(peekToken())) {
    std::string code_str = nextToken();
    std::istringstream iss(code_str);
    int code;
    iss >> code;
    // ステータスコードは100-599の範囲
    if (code < STATUS_CODE_MIN || code > STATUS_CODE_MAX) {
      throw std::runtime_error(
          makeError("invalid status code (must be 100-599): " + code_str));
    }
    codes.push_back(code);
  }

  // コードが1つもない場合はエラー
  if (codes.empty()) {
    throw std::runtime_error(
        makeError("error_page requires at least one status code"));
  }

  // パスが無い場合はエラー
  if (peekToken() == ";") {
    throw std::runtime_error(makeError("error_page requires a URI/path"));
  }

  // パスを取得
  std::string path = nextToken();
  skipSemicolon();

  // 全コードに同じパスを設定
  for (size_t i = 0; i < codes.size(); ++i) {
    server.error_pages[codes[i]] = path;
  }
}

void ConfigParser::parseClientMaxBodySizeDirective(ServerConfig& server) {
  std::string size_str = nextToken();
  server.client_max_body_size = parseSize(size_str);
  skipSemicolon();
}

void ConfigParser::parseServerRootDirective(ServerConfig& server) {
  server.root = nextToken();
  skipSemicolon();
}

// ============================================================================
// パーサ（location ディレクティブ）
// ============================================================================

void ConfigParser::parseRootDirective(LocationConfig& location) {
  location.root = nextToken();
  skipSemicolon();
}

void ConfigParser::parseAliasDirective(LocationConfig& location) {
  location.alias = nextToken();
  skipSemicolon();
}

void ConfigParser::parseIndexDirective(LocationConfig& location) {
  location.index = nextToken();
  skipSemicolon();
}

void ConfigParser::parseAutoindexDirective(LocationConfig& location) {
  std::string value = nextToken();
  if (value == "on") {
    location.autoindex = true;
  } else if (value == "off") {
    location.autoindex = false;
  } else {
    throw std::runtime_error(
        makeError("autoindex must be 'on' or 'off', got: " + value));
  }
  skipSemicolon();
}

void ConfigParser::parseAllowedMethodsDirective(LocationConfig& location) {
  location.allow_methods.clear();

  while (hasMoreTokens() && peekToken() != ";") {
    std::string method = nextToken();
    HttpMethod m;
    if (method == "GET") {
      m = GET;
    } else if (method == "POST") {
      m = POST;
    } else if (method == "DELETE") {
      m = DELETE;
    } else {
      throw std::runtime_error(makeError("unknown HTTP method: " + method));
    }
    // 重複チェック（重複は除外）
    bool exists = false;
    for (size_t i = 0; i < location.allow_methods.size(); ++i) {
      if (location.allow_methods[i] == m) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      location.allow_methods.push_back(m);
    }
  }
  skipSemicolon();
}

void ConfigParser::parseUploadPathDirective(LocationConfig& location) {
  location.upload_path = nextToken();
  skipSemicolon();
}

void ConfigParser::parseCgiExtensionDirective(LocationConfig& location) {
  location.cgi_extension = nextToken();
  skipSemicolon();
}

void ConfigParser::parseCgiPathDirective(LocationConfig& location) {
  location.cgi_path = nextToken();
  skipSemicolon();
}

void ConfigParser::parseReturnDirective(LocationConfig& location) {
  // return 301 http://example.com; 形式
  std::string code_str = nextToken();
  std::istringstream iss(code_str);
  int code;
  if (!(iss >> code)) {
    throw std::runtime_error(
        makeError("invalid return status code: " + code_str));
  }
  // リダイレクトステータスは300-399の範囲
  if (code < REDIRECT_CODE_MIN || code > REDIRECT_CODE_MAX) {
    throw std::runtime_error(
        makeError("return status code must be 300-399, got: " + code_str));
  }

  std::string url = nextToken();
  skipSemicolon();

  location.return_redirect = std::make_pair(code, url);
}

// ============================================================================
// ユーティリティ
// ============================================================================

size_t ConfigParser::parseSize(const std::string& size_str) const {
  if (size_str.empty()) {
    throw std::runtime_error(makeError("empty size string"));
  }

  size_t multiplier = 1;
  std::string num_str = size_str;
  char last_char = size_str[size_str.length() - 1];

  if (last_char == 'K' || last_char == 'k') {
    multiplier = KILOBYTE;
    num_str = size_str.substr(0, size_str.length() - 1);
  } else if (last_char == 'M' || last_char == 'm') {
    multiplier = MEGABYTE;
    num_str = size_str.substr(0, size_str.length() - 1);
  } else if (last_char == 'G' || last_char == 'g') {
    multiplier = GIGABYTE;
    num_str = size_str.substr(0, size_str.length() - 1);
  }

  std::istringstream iss(num_str);
  size_t value;
  if (!(iss >> value)) {
    throw std::runtime_error(makeError("invalid size: " + size_str));
  }

  // オーバーフローチェック
  size_t max_value = static_cast<size_t>(-1);
  if (value > max_value / multiplier) {
    throw std::runtime_error(makeError("size overflow: " + size_str));
  }

  return value * multiplier;
}

bool ConfigParser::isNumber(const std::string& str) const {
  if (str.empty()) {
    return false;
  }
  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] < '0' || str[i] > '9') {
      return false;
    }
  }
  return true;
}

std::string ConfigParser::makeError(const std::string& message) const {
  std::ostringstream oss;
  oss << file_path_ << ":" << last_line_ << ": " << message;
  return oss.str();
}
