#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

// ============================================================================
// コンストラクタ・デストラクタ
// ============================================================================

ConfigParser::ConfigParser(const std::string& file_path)
    : file_path_(file_path), current_index_(0), current_line_(1) {}

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
// ============================================================================

void ConfigParser::tokenize() {
  std::ifstream file(file_path_.c_str());
  if (!file.is_open()) {
    throw std::runtime_error("failed to open config file: " + file_path_);
  }

  std::string content;
  std::string line;
  while (std::getline(file, line)) {
    // コメント除去
    size_t comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }
    content += line + "\n";
  }
  file.close();

  std::string token;
  for (size_t i = 0; i < content.length(); ++i) {
    char c = content[i];

    if (isDelimiter(c)) {
      // 現在のトークンを保存
      if (!token.empty()) {
        tokens_.push_back(token);
        token.clear();
      }
      // 区切り文字自体もトークンとして保存（空白以外）
      if (c == '{' || c == '}' || c == ';') {
        tokens_.push_back(std::string(1, c));
      }
    } else {
      token += c;
    }
  }

  // 最後のトークン
  if (!token.empty()) {
    tokens_.push_back(token);
  }
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

  while (hasMoreTokens() && peekToken() != "}") {
    std::string directive = nextToken();

    if (directive == "listen") {
      parseListenDirective(server);
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

  expectToken("}");
  config.servers.push_back(server);
}

void ConfigParser::parseLocationBlock(ServerConfig& server) {
  // location パスを取得
  std::string path = nextToken();
  expectToken("{");

  LocationConfig location;
  location.path = path;

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
      parseReturnDirective(location);
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
  std::string port_str = nextToken();

  // host:port 形式の場合
  size_t colon_pos = port_str.find(':');
  if (colon_pos != std::string::npos) {
    server.host = port_str.substr(0, colon_pos);
    port_str = port_str.substr(colon_pos + 1);
  }

  // ポート番号を数値に変換
  std::istringstream iss(port_str);
  int port;
  if (!(iss >> port) || port < 0 || port > 65535) {
    throw std::runtime_error(makeError("invalid port number: " + port_str));
  }
  server.listen_port = port;

  skipSemicolon();
}

void ConfigParser::parseServerNameDirective(ServerConfig& server) {
  // セミコロンまで複数のサーバー名を読む
  while (hasMoreTokens() && peekToken() != ";") {
    server.server_names.push_back(nextToken());
  }
  skipSemicolon();
}

void ConfigParser::parseErrorPageDirective(ServerConfig& server) {
  // error_page 404 500 502 /error.html; の形式
  std::vector<int> codes;

  // エラーコードを集める（数字の間）
  while (hasMoreTokens() && peekToken() != ";" && isNumber(peekToken())) {
    std::istringstream iss(nextToken());
    int code;
    iss >> code;
    codes.push_back(code);
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
    if (method == "GET") {
      location.allow_methods.push_back(GET);
    } else if (method == "POST") {
      location.allow_methods.push_back(POST);
    } else if (method == "DELETE") {
      location.allow_methods.push_back(DELETE);
    } else {
      throw std::runtime_error(makeError("unknown HTTP method: " + method));
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
    multiplier = 1024;
    num_str = size_str.substr(0, size_str.length() - 1);
  } else if (last_char == 'M' || last_char == 'm') {
    multiplier = 1024 * 1024;
    num_str = size_str.substr(0, size_str.length() - 1);
  } else if (last_char == 'G' || last_char == 'g') {
    multiplier = 1024 * 1024 * 1024;
    num_str = size_str.substr(0, size_str.length() - 1);
  }

  std::istringstream iss(num_str);
  size_t value;
  if (!(iss >> value)) {
    throw std::runtime_error(makeError("invalid size: " + size_str));
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
  oss << file_path_ << ": " << message;
  return oss.str();
}
