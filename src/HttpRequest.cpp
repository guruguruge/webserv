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

  // chunkedパース用状態リセット
  _chunkState = CHUNK_SIZE_LINE;
  _currentChunkSize = 0;
  _chunkBytesRead = 0;
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
// getMaxBodySize - client_max_body_size を取得（ヘルパー）
// =============================================================================
size_t HttpRequest::getMaxBodySize() const {
  return (_config != NULL) ? _config->client_max_body_size
                           : DEFAULT_CLIENT_MAX_BODY_SIZE;
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
        // RFC 7230: Transfer-Encoding の値は大文字小文字を区別しない
        if (toLower(transferEncoding) == "chunked") {
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
        if (_contentLength > getMaxBodySize()) {
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
  if (_body.size() == _contentLength) {
    _parseState = REQ_COMPLETE;
  } else if (_body.size() > _contentLength) {
    setError(ERR_CONTENT_LENGTH_FORMAT);
  }
  // まだ足りない場合は REQ_BODY のまま、次の feed() を待つ
}

// =============================================================================
// parseBodyChunked - Transfer-Encoding: chunked のボディ解析
// =============================================================================
void HttpRequest::parseBodyChunked() {
  bool progress = true;

  while (progress && _parseState == REQ_BODY) {
    progress = false;

    switch (_chunkState) {
      case CHUNK_SIZE_LINE:
        if (parseChunkSizeLine())
          progress = true;
        break;
      case CHUNK_DATA:
        if (parseChunkData())
          progress = true;
        break;
      case CHUNK_DATA_CRLF:
        if (parseChunkDataCRLF())
          progress = true;
        break;
      case CHUNK_FINAL_CRLF:
        if (parseChunkFinalCRLF())
          progress = true;
        break;
    }
  }
}

// =============================================================================
// parseChunkSizeLine - チャンクサイズ行 "<hex>\r\n" をパース
// 戻り値: true=進捗あり, false=データ不足で待機
// =============================================================================
bool HttpRequest::parseChunkSizeLine() {
  // 1. \r\n を探す
  std::string::size_type pos = _buffer.find("\r\n");
  if (pos == std::string::npos) {
    return false;  // まだ行が揃っていない
  }

  // 2. 16進数文字列を取得
  std::string hexStr = _buffer.substr(0, pos);
  _buffer.erase(0, pos + 2);

  // chunk-extension がある場合はセミコロン以降を無視 (RFC 7230)
  std::string::size_type semiPos = hexStr.find(';');
  if (semiPos != std::string::npos) {
    hexStr = hexStr.substr(0, semiPos);
  }

  // 空文字列チェック
  if (hexStr.empty()) {
    setError(ERR_CONTENT_LENGTH_FORMAT);
    return false;
  }

  // 不正な16進数文字チェック
  for (std::string::size_type i = 0; i < hexStr.size(); ++i) {
    char c = hexStr[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) {
      setError(ERR_CONTENT_LENGTH_FORMAT);
      return false;
    }
  }

  // 3. std::hex を使って16進数をパース
  std::istringstream iss(hexStr);
  iss >> std::hex >> _currentChunkSize;
  if (iss.fail()) {
    setError(ERR_CONTENT_LENGTH_FORMAT);
    return false;
  }

  // 4. サイズ0なら終端チャンク
  if (_currentChunkSize == 0) {
    _chunkState = CHUNK_FINAL_CRLF;
    return true;
  }

  // 5. ボディサイズ制限チェック
  if (_body.size() + _currentChunkSize > getMaxBodySize()) {
    setError(ERR_BODY_TOO_LARGE);
    return false;
  }

  // 6. データ読み取りへ遷移
  _chunkBytesRead = 0;
  _chunkState = CHUNK_DATA;
  return true;
}

// =============================================================================
// parseChunkData - チャンクデータを読み取り
// 戻り値: true=進捗あり, false=データ不足で待機
// =============================================================================
bool HttpRequest::parseChunkData() {
  // TODO: 実装
  // 1. _currentChunkSize - _chunkBytesRead 分のデータを読み取り
  // 2. 完了したら CHUNK_DATA_CRLF へ
  return false;
}

// =============================================================================
// parseChunkDataCRLF - チャンクデータ後の \r\n を消費
// 戻り値: true=進捗あり, false=データ不足で待機
// =============================================================================
bool HttpRequest::parseChunkDataCRLF() {
  // TODO: 実装
  // 1. \r\n を確認して消費
  // 2. CHUNK_SIZE_LINE へ戻る
  return false;
}

// =============================================================================
// parseChunkFinalCRLF - 終端チャンク後の \r\n を消費 (trailer対応)
// 戻り値: true=進捗あり, false=データ不足で待機
// =============================================================================
bool HttpRequest::parseChunkFinalCRLF() {
  // TODO: 実装
  // 1. \r\n なら完了 (REQ_COMPLETE)
  // 2. それ以外はtrailer headerとしてスキップ
  return false;
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
