#include "../inc/Http.hpp"

// =============================================================================
// Default Constructor
// =============================================================================
HttpRequest::HttpRequest() : _config(NULL), _location(NULL) {
  clear();
}

// =============================================================================
// Destructor
// =============================================================================
HttpRequest::~HttpRequest() {
  // 動的リソースは持っていないので特に何もしない
}

// =============================================================================
// clear - 全メンバを初期状態にリセット（Keep-Alive用）
// =============================================================================
void HttpRequest::clear() {
  _buffer.clear();
  _parseState = REQ_REQUEST_LINE;
  _error = ERR_NONE;

  _method = UNKNOWN_METHOD;
  _path.clear();
  _query.clear();
  _version.clear();
  _headers.clear();
  _body.clear();
}

// =============================================================================
// feed - データを追加しパースを進める（中枢関数）
// =============================================================================
bool HttpRequest::feed(const char* data, size_t size) {
  // 1. バッファに追加
  _buffer.append(data, size);

  // 2. 状態に応じて進められるだけ進める
  bool progress = true;
  while (progress && _parseState != REQ_COMPLETE && _parseState != REQ_ERROR) {
    ParseState prev = _parseState;
    switch (_parseState) {
      case REQ_REQUEST_LINE:
        parseRequestLine();
        break;
      case REQ_HEADERS:
        parseHeaders();
        break;
      case REQ_BODY:
        parseBody();
        break;
      default:
        break;
    }
    // 状態が変わったら progress=true で続行
    progress = (prev != _parseState);
  }
  return isComplete();
}

// =============================================================================
// isComplete - パースが完了したかどうか
// =============================================================================
bool HttpRequest::isComplete() const {
  return (_parseState == REQ_COMPLETE);
}

// =============================================================================
// hasError - パースエラーが発生したかどうか
// =============================================================================
bool HttpRequest::hasError() const {
  return (_parseState == REQ_ERROR || _error != ERR_NONE);
}

// =============================================================================
// parseRequestLine - リクエストライン解析（TODO: 次ステップで実装）
// =============================================================================
void HttpRequest::parseRequestLine() {
  // TODO: "GET /path HTTP/1.1\r\n" を解析
  // 完了したら _parseState = REQ_HEADERS に遷移
}

// =============================================================================
// parseHeaders - ヘッダー解析（TODO: 次ステップで実装）
// =============================================================================
void HttpRequest::parseHeaders() {
  // TODO: "Header-Name: value\r\n" を解析
  // 空行 "\r\n" が来たら REQ_BODY or REQ_COMPLETE に遷移
}

// =============================================================================
// parseBody - ボディ解析（TODO: 次ステップで実装）
// =============================================================================
void HttpRequest::parseBody() {
  // TODO: Content-Length または chunked encoding に基づいてボディを読み取り
  // 完了したら _parseState = REQ_COMPLETE に遷移
}

// =============================================================================
// Getter implementations
// =============================================================================
HttpMethod HttpRequest::getMethod() const {
  return _method;
}

std::string HttpRequest::getPath() const {
  return _path;
}

std::string HttpRequest::getHeader(const std::string& key) const {
  std::map<std::string, std::string>::const_iterator it = _headers.find(key);
  if (it != _headers.end()) {
    return it->second;
  }
  return "";
}

const std::vector<char>& HttpRequest::getBody() const {
  return _body;
}

// =============================================================================
// Config setters/getters
// =============================================================================
void HttpRequest::setConfig(const ServerConfig* config) {
  _config = config;
}

const ServerConfig* HttpRequest::getConfig() const {
  return _config;
}
