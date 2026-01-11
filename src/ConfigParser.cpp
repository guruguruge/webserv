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
    : _file_path(file_path), _current_index(0), _last_line(1) {}

ConfigParser::~ConfigParser() {}

// ============================================================================
// メインパース関数
// ============================================================================

void ConfigParser::parse(MainConfig& config) {
  _tokenize();

  while (_hasMoreTokens()) {
    std::string token = _peekToken();
    if (token == "server") {
      _parseServerBlock(config);
    } else if (token == "#") {
      // 通常はトークナイズ時（tokenize）でコメントが除去されるが、
      // 予期せぬ '#' トークンが残っていた場合に備えた防御的なチェック
      _nextToken();
    } else {
      throw std::runtime_error(_makeError(
          "expected 'server' directive at top level, got: " + token));
    }
  }
}

// ============================================================================
// トークナイザ
// 注意: クォートで囲まれた文字列は非対応。
//       スペースを含むパス（例: root "/var/www/my site";)は使用不可。
// ============================================================================

void ConfigParser::_tokenize() {
  // 状態を初期化（複数回呼び出しに対応）
  _tokens.clear();
  _token_lines.clear();
  _current_index = 0;
  _last_line = 1;

  std::ifstream file(_file_path.c_str());
  if (!file.is_open()) {
    throw std::runtime_error("failed to open config file: " + _file_path);
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

      if (_isDelimiter(c)) {
        // 現在のトークンを保存
        if (!token.empty()) {
          _tokens.push_back(token);
          _token_lines.push_back(line_number);
          token.clear();
        }
        // 区切り文字自体もトークンとして保存（空白以外）
        if (c == '{' || c == '}' || c == ';') {
          _tokens.push_back(std::string(1, c));
          _token_lines.push_back(line_number);
        }
      } else {
        token += c;
      }
    }

    // 行末のトークン
    if (!token.empty()) {
      _tokens.push_back(token);
      _token_lines.push_back(line_number);
    }
  }
  file.close();
}

bool ConfigParser::_isDelimiter(char c) const {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '{' ||
         c == '}' || c == ';';
}

// ============================================================================
// トークン操作
// ============================================================================

std::string ConfigParser::_nextToken() {
  if (_current_index >= _tokens.size()) {
    throw std::runtime_error(_makeError("unexpected end of file"));
  }
  _last_line = _token_lines[_current_index];
  return _tokens[_current_index++];
}

std::string ConfigParser::_peekToken() const {
  if (_current_index >= _tokens.size()) {
    return "";
  }
  return _tokens[_current_index];
}

bool ConfigParser::_hasMoreTokens() const {
  return _current_index < _tokens.size();
}

void ConfigParser::_expectToken(const std::string& expected) {
  std::string token = _nextToken();
  if (token != expected) {
    throw std::runtime_error(
        _makeError("expected '" + expected + "', got '" + token + "'"));
  }
}

void ConfigParser::_skipSemicolon() {
  _expectToken(";");
}

// ============================================================================
// パーサ（ブロック）
// ============================================================================

void ConfigParser::_parseServerBlock(MainConfig& config) {
  _expectToken("server");
  _expectToken("{");

  ServerConfig server;
  bool has_listen = false;

  while (_hasMoreTokens() && _peekToken() != "}") {
    std::string directive = _nextToken();

    if (directive == "listen") {
      if (has_listen) {
        throw std::runtime_error(_makeError("duplicate 'listen' directive"));
      }
      _parseListenDirective(server);
      has_listen = true;
    } else if (directive == "server_name") {
      _parseServerNameDirective(server);
    } else if (directive == "root") {
      _parseServerRootDirective(server);
    } else if (directive == "error_page") {
      _parseErrorPageDirective(server);
    } else if (directive == "client_max_body_size") {
      _parseClientMaxBodySizeDirective(server);
    } else if (directive == "location") {
      _parseLocationBlock(server);
    } else {
      throw std::runtime_error(
          _makeError("unknown server directive: " + directive));
    }
  }

  // listenディレクティブは必須
  if (!has_listen) {
    throw std::runtime_error(
        _makeError("'listen' directive is required in server block"));
  }

  _expectToken("}");
  config.servers.push_back(server);
}

void ConfigParser::_parseLocationBlock(ServerConfig& server) {
  // location パスを取得
  std::string path = _nextToken();

  // 重複locationチェック
  for (size_t i = 0; i < server.locations.size(); ++i) {
    if (server.locations[i].path == path) {
      throw std::runtime_error(_makeError("duplicate location path: " + path));
    }
  }

  _expectToken("{");

  LocationConfig location;
  location.path = path;
  bool has_return = false;

  while (_hasMoreTokens() && _peekToken() != "}") {
    std::string directive = _nextToken();

    if (directive == "root") {
      _parseRootDirective(location);
    } else if (directive == "alias") {
      _parseAliasDirective(location);
    } else if (directive == "index") {
      _parseIndexDirective(location);
    } else if (directive == "autoindex") {
      _parseAutoindexDirective(location);
    } else if (directive == "allowed_methods") {
      _parseAllowedMethodsDirective(location);
    } else if (directive == "upload_path") {
      _parseUploadPathDirective(location);
    } else if (directive == "cgi_extension") {
      _parseCgiExtensionDirective(location);
    } else if (directive == "cgi_path") {
      _parseCgiPathDirective(location);
    } else if (directive == "return") {
      if (has_return) {
        throw std::runtime_error(_makeError("duplicate 'return' directive"));
      }
      _parseReturnDirective(location);
      has_return = true;
    } else {
      throw std::runtime_error(
          _makeError("unknown location directive: " + directive));
    }
  }

  _expectToken("}");
  server.locations.push_back(location);
}

// ============================================================================
// パーサ（server ディレクティブ）
// ============================================================================

void ConfigParser::_parseListenDirective(ServerConfig& server) {
  std::string value = _nextToken();

  // IPv6は非対応（'['を含む場合はエラー）
  if (value.find('[') != std::string::npos) {
    throw std::runtime_error(
        _makeError("IPv6 addresses are not supported: " + value));
  }

  size_t colon_pos = value.find(':');

  // host:port 形式の場合
  if (colon_pos != std::string::npos) {
    // コロンが複数ある場合はエラー
    if (value.find(':', colon_pos + 1) != std::string::npos) {
      throw std::runtime_error(
          _makeError("invalid listen format (multiple colons): " + value));
    }
    // hostまたはportが空の場合はエラー
    if (colon_pos == 0 || colon_pos == value.size() - 1) {
      throw std::runtime_error(
          _makeError("invalid listen format (empty host or port): " + value));
    }

    server.host = value.substr(0, colon_pos);
    std::string port_str = value.substr(colon_pos + 1);

    int port;
    if (!_tryParsePort(port_str, port)) {
      throw std::runtime_error(_makeError("invalid port number: " + port_str));
    }
    server.listen_port = port;
  }
  // コロンがない場合: port のみ or host のみ
  else {
    int port;
    // 数値のみ → ポート番号として扱う (例: "8080")
    if (_tryParsePort(value, port)) {
      server.listen_port = port;
    }
    // 数値でない → ホスト名として扱う (例: "localhost", "192.0.2.1")
    // デフォルトポート80を使用
    else {
      server.host = value;
      server.listen_port = 80;
    }
  }

  _skipSemicolon();
}

void ConfigParser::_parseServerNameDirective(ServerConfig& server) {
  // セミコロンまで複数のサーバー名を読む
  while (_hasMoreTokens() && _peekToken() != ";") {
    std::string name = _nextToken();
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
  _skipSemicolon();
}

void ConfigParser::_parseErrorPageDirective(ServerConfig& server) {
  // error_page 404 500 502 /error.html; の形式
  std::vector<int> codes;

  // エラーコードを集める（数字の間）
  while (_hasMoreTokens() && _peekToken() != ";" && _isNumber(_peekToken())) {
    std::string code_str = _nextToken();
    std::istringstream iss(code_str);
    int code;
    if (!(iss >> code)) {
      throw std::runtime_error(_makeError("invalid status code: " + code_str));
    }
    // ステータスコードは100-599の範囲
    if (code < STATUS_CODE_MIN || code > STATUS_CODE_MAX) {
      throw std::runtime_error(
          _makeError("invalid status code (must be 100-599): " + code_str));
    }
    codes.push_back(code);
  }

  // コードが1つもない場合はエラー
  if (codes.empty()) {
    throw std::runtime_error(
        _makeError("error_page requires at least one status code"));
  }

  // パスが無い場合はエラー
  if (_peekToken() == ";") {
    throw std::runtime_error(_makeError("error_page requires a URI/path"));
  }

  // パスを取得
  std::string path = _nextToken();
  _skipSemicolon();

  // 全コードに同じパスを設定
  for (size_t i = 0; i < codes.size(); ++i) {
    server.error_pages[codes[i]] = path;
  }
}

void ConfigParser::_parseClientMaxBodySizeDirective(ServerConfig& server) {
  std::string size_str = _nextToken();
  server.client_max_body_size = _parseSize(size_str);
  _skipSemicolon();
}

void ConfigParser::_parseServerRootDirective(ServerConfig& server) {
  if (_peekToken() == ";") {
    throw std::runtime_error(_makeError("root directive requires a path"));
  }
  server.root = _nextToken();
  _skipSemicolon();
}

// ============================================================================
// パーサ（location ディレクティブ）
// ============================================================================

void ConfigParser::_parseRootDirective(LocationConfig& location) {
  if (_peekToken() == ";") {
    throw std::runtime_error(_makeError("root directive requires a path"));
  }
  location.root = _nextToken();
  _skipSemicolon();
}

void ConfigParser::_parseAliasDirective(LocationConfig& location) {
  if (_peekToken() == ";") {
    throw std::runtime_error(_makeError("alias directive requires a path"));
  }
  location.alias = _nextToken();
  _skipSemicolon();
}

void ConfigParser::_parseIndexDirective(LocationConfig& location) {
  if (_peekToken() == ";") {
    throw std::runtime_error(_makeError("index directive requires a filename"));
  }
  location.index = _nextToken();
  _skipSemicolon();
}

void ConfigParser::_parseAutoindexDirective(LocationConfig& location) {
  std::string value = _nextToken();
  if (value == "on") {
    location.autoindex = true;
  } else if (value == "off") {
    location.autoindex = false;
  } else {
    throw std::runtime_error(
        _makeError("autoindex must be 'on' or 'off', got: " + value));
  }
  _skipSemicolon();
}

void ConfigParser::_parseAllowedMethodsDirective(LocationConfig& location) {
  location.allow_methods.clear();

  while (_hasMoreTokens() && _peekToken() != ";") {
    std::string method = _nextToken();
    HttpMethod m;
    if (method == "GET") {
      m = GET;
    } else if (method == "POST") {
      m = POST;
    } else if (method == "DELETE") {
      m = DELETE;
    } else {
      throw std::runtime_error(_makeError("unknown HTTP method: " + method));
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
  _skipSemicolon();
}

void ConfigParser::_parseUploadPathDirective(LocationConfig& location) {
  if (_peekToken() == ";") {
    throw std::runtime_error(
        _makeError("upload_path directive requires a path"));
  }
  location.upload_path = _nextToken();
  _skipSemicolon();
}

void ConfigParser::_parseCgiExtensionDirective(LocationConfig& location) {
  if (_peekToken() == ";") {
    throw std::runtime_error(
        _makeError("cgi_extension directive requires an extension"));
  }
  location.cgi_extension = _nextToken();
  _skipSemicolon();
}

void ConfigParser::_parseCgiPathDirective(LocationConfig& location) {
  if (_peekToken() == ";") {
    throw std::runtime_error(_makeError("cgi_path directive requires a path"));
  }
  location.cgi_path = _nextToken();
  _skipSemicolon();
}

void ConfigParser::_parseReturnDirective(LocationConfig& location) {
  // return 301 http://example.com; 形式
  std::string code_str = _nextToken();
  std::istringstream iss(code_str);
  int code;
  if (!(iss >> code)) {
    throw std::runtime_error(
        _makeError("invalid return status code: " + code_str));
  }
  // リダイレクトステータスは300-399の範囲
  if (code < REDIRECT_CODE_MIN || code > REDIRECT_CODE_MAX) {
    throw std::runtime_error(
        _makeError("return status code must be 300-399, got: " + code_str));
  }

  std::string url = _nextToken();
  _skipSemicolon();

  location.return_redirect = std::make_pair(code, url);
}

// ============================================================================
// ユーティリティ
// ============================================================================

size_t ConfigParser::_parseSize(const std::string& size_str) const {
  if (size_str.empty()) {
    throw std::runtime_error(_makeError("empty size string"));
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
  char remaining;

  // 数値としてパース可能で、余分な文字がないかチェック
  if (!(iss >> value) || iss.get(remaining)) {
    throw std::runtime_error(_makeError("invalid size: " + size_str));
  }

  // オーバーフローチェック
  size_t max_value = static_cast<size_t>(-1);
  if (value > max_value / multiplier) {
    throw std::runtime_error(_makeError("size overflow: " + size_str));
  }

  return value * multiplier;
}

bool ConfigParser::_tryParsePort(const std::string& str, int& port) const {
  if (str.empty()) {
    return false;
  }

  std::istringstream iss(str);
  char remaining;

  // 数値としてパース可能で、余分な文字がなく、範囲内かチェック
  if ((iss >> port) && !iss.get(remaining) && port >= PORT_MIN &&
      port <= PORT_MAX) {
    return true;
  }
  return false;
}

bool ConfigParser::_isNumber(const std::string& str) const {
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

std::string ConfigParser::_makeError(const std::string& message) const {
  std::ostringstream oss;
  oss << _file_path << ":" << _last_line << ": " << message;
  return oss.str();
}
