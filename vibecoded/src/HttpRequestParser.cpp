#include "HttpRequestParser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

HttpRequestParser::HttpRequestParser(size_t maxBodySize)
    : _state(PARSE_REQUEST_LINE),
      _request(),
      _buffer(""),
      _errorMessage(""),
      _maxBodySize(maxBodySize),
      _currentChunkSize(0),
      _currentChunkRead(0) {}

HttpRequestParser::~HttpRequestParser() {}

bool HttpRequestParser::parse(const char* data, size_t len) {
  // エラー状態の場合は何もしない
  if (_state == PARSE_ERROR || _state == PARSE_DONE) {
    return _state == PARSE_DONE;
  }

  // データをバッファに追加
  _buffer.append(data, len);

  // 状態機械でパース
  while (_state != PARSE_DONE && _state != PARSE_ERROR) {
    bool progress = false;

    switch (_state) {
      case PARSE_REQUEST_LINE:
        progress = parseRequestLine();
        break;
      case PARSE_HEADERS:
        progress = parseHeaders();
        break;
      case PARSE_BODY:
        progress = parseBody();
        break;
      case PARSE_CHUNK_SIZE:
        progress = parseChunkSize();
        break;
      case PARSE_CHUNK_DATA:
        progress = parseChunkData();
        break;
      case PARSE_CHUNK_TRAILER:
        progress = parseChunkTrailer();
        break;
      default:
        setError("Invalid parser state");
        return false;
    }

    // 進捗がない場合はデータ待ち
    if (!progress) {
      break;
    }
  }

  return _state != PARSE_ERROR;
}

HttpRequestParser::ParseState HttpRequestParser::getState() const {
  return _state;
}

const HttpRequest& HttpRequestParser::getRequest() const {
  return _request;
}

const std::string& HttpRequestParser::getErrorMessage() const {
  return _errorMessage;
}

void HttpRequestParser::reset() {
  _state = PARSE_REQUEST_LINE;
  _request = HttpRequest();
  _buffer.clear();
  _errorMessage.clear();
  _currentChunkSize = 0;
  _currentChunkRead = 0;
}

// ========================================
// 内部パース関数
// ========================================

bool HttpRequestParser::parseRequestLine() {
  // リクエストラインの終端を探す
  size_t pos = _buffer.find("\r\n");
  if (pos == std::string::npos) {
    // まだ完全な行が来ていない
    if (_buffer.size() > 8192) {
      setError("Request line too long");
      return false;
    }
    return false;
  }

  // リクエストラインを抽出
  std::string line = _buffer.substr(0, pos);
  _buffer.erase(0, pos + 2);

  // リクエストラインを分割: "METHOD URI HTTP/VERSION"
  std::istringstream iss(line);
  if (!(iss >> _request.method >> _request.uri >> _request.httpVersion)) {
    setError("Malformed request line");
    return false;
  }

  // メソッド検証
  if (!isValidMethod(_request.method)) {
    setError("Invalid HTTP method: " + _request.method);
    return false;
  }

  // HTTPバージョン検証
  if (_request.httpVersion != "HTTP/1.1" &&
      _request.httpVersion != "HTTP/1.0") {
    setError("Unsupported HTTP version: " + _request.httpVersion);
    return false;
  }

  // URIからpath/queryを分離
  processUri();

  // 次の状態へ
  _state = PARSE_HEADERS;
  return true;
}

bool HttpRequestParser::parseHeaders() {
  while (true) {
    // ヘッダー行の終端を探す
    size_t pos = _buffer.find("\r\n");
    if (pos == std::string::npos) {
      // まだ完全な行が来ていない
      if (_buffer.size() > 8192) {
        setError("Header line too long");
        return false;
      }
      return false;
    }

    // ヘッダー行を抽出
    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);

    // 空行 = ヘッダー終了
    if (line.empty()) {
      // Hostヘッダーから情報を抽出
      processHost();

      // Transfer-Encodingのチェック
      std::map<std::string, std::string>::const_iterator it =
          _request.headers.find("transfer-encoding");
      if (it != _request.headers.end() &&
          it->second.find("chunked") != std::string::npos) {
        _state = PARSE_CHUNK_SIZE;
        return true;
      }

      // Content-Lengthのチェック
      it = _request.headers.find("content-length");
      if (it != _request.headers.end()) {
        std::istringstream iss(it->second);
        size_t contentLength;
        if (!(iss >> contentLength)) {
          setError("Invalid Content-Length");
          return false;
        }

        // ボディサイズチェック
        if (contentLength > _maxBodySize) {
          setError("Request body too large");
          return false;
        }

        _request.body.reserve(contentLength);
        _state = PARSE_BODY;
        return true;
      }

      // ボディがない場合は完了
      _state = PARSE_DONE;
      return true;
    }

    // ヘッダーフィールドをパース: "Key: Value"
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
      setError("Malformed header line");
      return false;
    }

    std::string key = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);

    // 前後の空白を除去
    // keyの前後
    size_t start = key.find_first_not_of(" \t");
    size_t end = key.find_last_not_of(" \t");
    if (start != std::string::npos) {
      key = key.substr(start, end - start + 1);
    }

    // valueの前後
    start = value.find_first_not_of(" \t");
    end = value.find_last_not_of(" \t");
    if (start != std::string::npos) {
      value = value.substr(start, end - start + 1);
    } else {
      value = "";
    }

    // ヘッダーキーを小文字化して格納
    _request.headers[toLowerCase(key)] = value;
  }
}

bool HttpRequestParser::parseBody() {
  // Content-Lengthベースのボディパース
  std::map<std::string, std::string>::const_iterator it =
      _request.headers.find("content-length");
  if (it == _request.headers.end()) {
    setError("Content-Length header missing");
    return false;
  }

  std::istringstream iss(it->second);
  size_t contentLength;
  iss >> contentLength;

  // 必要なデータが全て揃っているかチェック
  if (_buffer.size() < contentLength) {
    // まだ全てのデータが届いていない
    return false;
  }

  // ボディデータをコピー
  _request.body.assign(_buffer.begin(), _buffer.begin() + contentLength);
  _buffer.erase(0, contentLength);

  _state = PARSE_DONE;
  return true;
}

bool HttpRequestParser::parseChunkSize() {
  // チャンクサイズ行を探す
  size_t pos = _buffer.find("\r\n");
  if (pos == std::string::npos) {
    if (_buffer.size() > 1024) {
      setError("Chunk size line too long");
      return false;
    }
    return false;
  }

  std::string line = _buffer.substr(0, pos);
  _buffer.erase(0, pos + 2);

  // チャンクサイズ行から16進数を抽出(拡張フィールドがある場合は無視)
  size_t semicolonPos = line.find(';');
  if (semicolonPos != std::string::npos) {
    line = line.substr(0, semicolonPos);
  }

  // 16進数をパース
  std::istringstream iss(line);
  if (!(iss >> std::hex >> _currentChunkSize)) {
    setError("Invalid chunk size");
    return false;
  }

  // チャンクサイズが0なら転送終了
  if (_currentChunkSize == 0) {
    _state = PARSE_DONE;
    return true;
  }

  // ボディサイズチェック
  if (_request.body.size() + _currentChunkSize > _maxBodySize) {
    setError("Request body too large (chunked)");
    return false;
  }

  _currentChunkRead = 0;
  _state = PARSE_CHUNK_DATA;
  return true;
}

bool HttpRequestParser::parseChunkData() {
  size_t remaining = _currentChunkSize - _currentChunkRead;
  size_t available = _buffer.size();

  if (available < remaining) {
    // チャンクデータがまだ完全に届いていない
    _request.body.insert(_request.body.end(), _buffer.begin(), _buffer.end());
    _currentChunkRead += available;
    _buffer.clear();
    return false;
  }

  // チャンクデータを完全に読み込む
  _request.body.insert(_request.body.end(), _buffer.begin(),
                       _buffer.begin() + remaining);
  _buffer.erase(0, remaining);
  _currentChunkRead = _currentChunkSize;

  _state = PARSE_CHUNK_TRAILER;
  return true;
}

bool HttpRequestParser::parseChunkTrailer() {
  // チャンク末尾の "\r\n" を確認
  if (_buffer.size() < 2) {
    return false;
  }

  if (_buffer[0] != '\r' || _buffer[1] != '\n') {
    setError("Invalid chunk trailer");
    return false;
  }

  _buffer.erase(0, 2);
  _state = PARSE_CHUNK_SIZE;
  return true;
}

// ========================================
// ヘルパー関数
// ========================================

void HttpRequestParser::processUri() {
  // URIからクエリ文字列を分離
  size_t questionPos = _request.uri.find('?');
  if (questionPos != std::string::npos) {
    _request.path = _request.uri.substr(0, questionPos);
    _request.query = _request.uri.substr(questionPos + 1);
  } else {
    _request.path = _request.uri;
    _request.query = "";
  }
}

void HttpRequestParser::processHost() {
  // Hostヘッダーを確認
  std::map<std::string, std::string>::const_iterator it =
      _request.headers.find("host");
  if (it == _request.headers.end()) {
    // HTTP/1.1ではHostヘッダーが必須
    if (_request.httpVersion == "HTTP/1.1") {
      setError("Missing Host header");
    }
    return;
  }

  std::string hostValue = it->second;

  // ホストとポートを分離: "localhost:8080" -> host="localhost", port="8080"
  size_t colonPos = hostValue.find(':');
  if (colonPos != std::string::npos) {
    _request.host = hostValue.substr(0, colonPos);
    _request.port = hostValue.substr(colonPos + 1);
  } else {
    _request.host = hostValue;
    _request.port = "";
  }
}

std::string HttpRequestParser::toLowerCase(const std::string& str) {
  std::string result = str;
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] = std::tolower(static_cast<unsigned char>(result[i]));
  }
  return result;
}

bool HttpRequestParser::isValidMethod(const std::string& method) {
  // HTTP/1.1の標準メソッド + 課題で必要なメソッド
  return method == "GET" || method == "POST" || method == "DELETE" ||
         method == "HEAD" || method == "PUT" || method == "OPTIONS" ||
         method == "TRACE" || method == "CONNECT" || method == "PATCH";
}

void HttpRequestParser::setError(const std::string& message) {
  _state = PARSE_ERROR;
  _errorMessage = message;
}
