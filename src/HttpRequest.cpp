#include "../inc/Http.hpp"

// =============================================================================
// ヘルパー関数: 文字列から HttpMethod への変換
// =============================================================================
static HttpMethod stringToMethod(const std::string& str) {
  if (str == "GET") {
    return GET;
  } else if (str == "POST") {
    return POST;
  } else if (str == "DELETE") {
    return DELETE;
  }
  return UNKNOWN_METHOD;
}

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
// parseRequestLine - リクエストライン解析
// =============================================================================
void HttpRequest::parseRequestLine() {
  // 1. _buffer から \r\n を探す
  std::string::size_type pos = _buffer.find("\r\n");
  if (pos == std::string::npos) {
    // 見つからなければ return（分割受信に備える）
    return;
  }

  // 2. 1行取り出す → "GET /path?query HTTP/1.1"
  std::string line = _buffer.substr(0, pos);
  _buffer.erase(0, pos + 2);  // \r\n の2バイトも消す

  // 3. スペース区切りで3つに分解
  std::istringstream iss(line);
  std::string methodStr, uri, versionStr;
  iss >> methodStr >> uri >> versionStr;

  // 3つ取れたか確認
  if (methodStr.empty() || uri.empty() || versionStr.empty()) {
    _error = ERR_INVALID_METHOD;
    _parseState = REQ_ERROR;
    return;
  }

  // 4. メソッドを HttpMethod に変換
  _method = stringToMethod(methodStr);
  if (_method == UNKNOWN_METHOD) {
    _error = ERR_INVALID_METHOD;
    _parseState = REQ_ERROR;
    return;
  }

  // 5. パスとクエリを分割（?の位置で）
  std::string::size_type queryPos = uri.find('?');
  if (queryPos == std::string::npos) {
    _path = uri;
    _query.clear();
  } else {
    _path = uri.substr(0, queryPos);
    _query = uri.substr(queryPos + 1);
  }

  // 6. バージョンをチェック
  if (versionStr != "HTTP/1.1" && versionStr != "HTTP/1.0") {
    _error = ERR_INVALID_VERSION;
    _parseState = REQ_ERROR;
    return;
  }
  _version = versionStr;

  // 成功 → ヘッダー解析へ
  _parseState = REQ_HEADERS;
}

// =============================================================================
// parseHeaders - ヘッダー解析
// =============================================================================
void HttpRequest::parseHeaders() {
  // 1. \r\n を探す
  std::string::size_type pos = _buffer.find("\r\n");
  if (pos == std::string::npos) {
    // 見つからなければ return（分割受信に備える）
    return;
  }

  // 2. 空行なら → ヘッダー終了、ボディへ遷移
  if (pos == 0) {
    _buffer.erase(0, 2);  // 空行 "\r\n" を消す
    // Content-Length があればボディへ、なければ完了
    std::string contentLength = getHeader("Content-Length");
    if (contentLength.empty()) {
      _parseState = REQ_COMPLETE;
    } else {
      _parseState = REQ_BODY;
    }
    return;
  }

  // 3. 1行取り出す
  std::string line = _buffer.substr(0, pos);
  _buffer.erase(0, pos + 2);  // \r\n の2バイトも消す

  // 4. ":" で分割して key: value を取得
  std::string::size_type colonPos = line.find(':');
  if (colonPos == std::string::npos) {
    // ":" がない → 不正なヘッダー（無視するか、エラーにするか）
    // ここでは無視して次へ進む
    return;
  }

  std::string key = line.substr(0, colonPos);
  std::string value = line.substr(colonPos + 1);

  // 5. value の先頭空白をトリム
  std::string::size_type start = value.find_first_not_of(" \t");
  if (start != std::string::npos) {
    value = value.substr(start);
  } else {
    value.clear();
  }

  // 6. _headers に格納
  _headers[key] = value;
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
