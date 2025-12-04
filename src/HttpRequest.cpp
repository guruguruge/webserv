#include <cctype>
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
// ヘルパー関数: 文字列を小文字に変換
// =============================================================================
static std::string toLower(const std::string& str) {
  std::string result = str;
  for (std::string::size_type i = 0; i < result.size(); ++i) {
    result[i] =
        static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
  }
  return result;
}

// =============================================================================
// ヘルパー関数: 文字列が数値のみかチェック
// =============================================================================
static bool isDigitsOnly(const std::string& str) {
  if (str.empty()) {
    return false;
  }
  for (std::string::size_type i = 0; i < str.size(); ++i) {
    if (str[i] < '0' || str[i] > '9') {
      return false;
    }
  }
  return true;
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

  // ヘッダーパース用カウンタリセット
  _headerCount = 0;
  _totalHeaderSize = 0;

  _method = UNKNOWN_METHOD;
  _path.clear();
  _query.clear();
  _version.clear();
  _headers.clear();
  _body.clear();
  _contentLength = 0;
  _isChunked = false;
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
// setError - エラー状態をセットしREQ_ERRORに遷移
// =============================================================================
void HttpRequest::setError(ErrorCode err) {
  _error = err;
  _parseState = REQ_ERROR;
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
  static const size_t MAX_HEADER_COUNT = 100;  // 最大100ヘッダー

  // 全てのヘッダー行をループで処理
  while (true) {
    // 1. \r\n を探す
    std::string::size_type pos = _buffer.find("\r\n");
    if (pos == std::string::npos) {
      // 見つからなければ return（分割受信に備える）
      return;
    }

    // ヘッダーサイズチェック
    _totalHeaderSize += pos + 2;
    if (_totalHeaderSize > MAX_HEADER_SIZE) {
      setError(ERR_HEADER_TOO_LARGE);
      return;
    }

    // 2. 空行なら → ヘッダー終了、ボディへ遷移
    if (pos == 0) {
      _buffer.erase(0, 2);  // 空行 "\r\n" を消す

      // HTTP/1.1 では Host ヘッダー必須
      if (_version == "HTTP/1.1" && getHeader("host").empty()) {
        setError(ERR_MISSING_HOST);
        return;
      }

      // Content-Length と Transfer-Encoding の同時指定は禁止 (RFC 7230)
      std::string contentLength = getHeader("Content-Length");
      std::string transferEncoding = getHeader("Transfer-Encoding");
      if (!contentLength.empty() && !transferEncoding.empty()) {
        setError(ERR_CONFLICTING_HEADERS);
        return;
      }

      // Content-Length の形式チェックとボディ状態の決定
      if (!transferEncoding.empty()) {
        // Transfer-Encoding が指定されている場合は "chunked" のみ許可
        if (transferEncoding == "chunked") {
          _isChunked = true;
          _parseState = REQ_BODY;
        } else {
          // "chunked" 以外はエラー (gzip, deflate 等は未サポート)
          setError(ERR_INVALID_TRANSFER_ENCODING);
          return;
        }
      } else if (contentLength.empty()) {
        // Content-Length なし → ボディなし
        _parseState = REQ_COMPLETE;
      } else {
        // Content-Length あり
        // 数値かどうか確認
        if (!isDigitsOnly(contentLength)) {
          setError(ERR_CONTENT_LENGTH_FORMAT);
          return;
        }
        // Content-Lengthの値をメンバ変数に代入
        std::istringstream iss(contentLength);
        iss >> _contentLength;
        // オーバーフロー検出
        if (iss.fail()) {
          setError(ERR_CONTENT_LENGTH_FORMAT);
          return;
        }
        // client_max_body_size との比較
        size_t maxBodySize = (_config != NULL) ? _config->client_max_body_size
                                               : DEFAULT_CLIENT_MAX_BODY_SIZE;
        if (_contentLength > maxBodySize) {
          setError(ERR_BODY_TOO_LARGE);
          return;
        }
        _parseState = REQ_BODY;
      }
      return;
    }

    // ヘッダー行数チェック
    ++_headerCount;
    if (_headerCount > MAX_HEADER_COUNT) {
      setError(ERR_HEADER_TOO_LARGE);
      return;
    }

    // 3. 1行取り出す
    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);  // \r\n の2バイトも消す

    // 4. ":" で分割して key: value を取得
    std::string::size_type colonPos = line.find(':');
    if (colonPos == std::string::npos) {
      // ":" がない → 不正なヘッダー（無視して次へ）
      continue;
    }

    std::string key = toLower(line.substr(0, colonPos));
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
}

// =============================================================================
// parseBody - ボディ解析のディスパッチャ
// =============================================================================
void HttpRequest::parseBody() {
  if (_isChunked) {
    parseBodyChunked();
  } else {
    parseBodyContentLength();
  }
}

// =============================================================================
// parseBodyContentLength - Content-Length ベースのボディ解析
// =============================================================================
void HttpRequest::parseBodyContentLength() {
  // Content-Length が 0 の場合は即完了
  if (_contentLength == 0) {
    _parseState = REQ_COMPLETE;
    return;
  }

  // 現在のボディサイズを計算
  size_t currentBodySize = _body.size();
  size_t remaining = _contentLength - currentBodySize;

  // バッファから読み取れる分を計算
  size_t toRead = _buffer.size();
  if (toRead > remaining) {
    toRead = remaining;
  }

  // バッファからボディへ転送
  if (toRead > 0) {
    _body.insert(_body.end(), _buffer.begin(), _buffer.begin() + toRead);
    _buffer.erase(0, toRead);
  }

  // ボディが完全に読み取れたかチェック
  if (_body.size() >= _contentLength) {
    _parseState = REQ_COMPLETE;
  }
  // まだ足りない場合は REQ_BODY のまま、次の feed() を待つ
}

// =============================================================================
// parseBodyChunked - Transfer-Encoding: chunked のボディ解析
// =============================================================================
void HttpRequest::parseBodyChunked() {
  // TODO: chunked encoding の実装
  // 各チャンクは以下の形式:
  //   <chunk-size in hex>\r\n
  //   <chunk-data>\r\n
  // 最後のチャンクは:
  //   0\r\n
  //   \r\n
  //
  // 実装時の考慮事項:
  // 1. チャンクサイズの16進数パース
  // 2. チャンクデータの読み取り
  // 3. 終端チャンク (size=0) の検出
  // 4. client_max_body_size のチェック (累積サイズ)
  // 5. trailer headers のスキップ (必要なら)
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
  std::map<std::string, std::string>::const_iterator it =
      _headers.find(toLower(key));
  if (it != _headers.end()) {
    return it->second;
  }
  return "";
}

const std::vector<char>& HttpRequest::getBody() const {
  return _body;
}

size_t HttpRequest::getContentLength() const {
  return _contentLength;
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
