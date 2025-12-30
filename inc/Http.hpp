#ifndef HTTP_HPP
#define HTTP_HPP

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include "Config.hpp"
#include "Defines.hpp"

// --- Error Codes ---
enum ErrorCode {
  ERR_NONE,
  ERR_INVALID_METHOD,
  ERR_INVALID_VERSION,
  ERR_URI_TOO_LONG,
  ERR_HEADER_TOO_LARGE,
  ERR_MISSING_HOST,
  ERR_CONTENT_LENGTH_FORMAT,
  ERR_CONFLICTING_HEADERS,
  ERR_BODY_TOO_LARGE,
  ERR_INVALID_TRANSFER_ENCODING,
  ERR_INVALID_CHUNK_FORMAT
  // 必要に応じて追加
};

// --- HTTP Request ---
// 受信バッファを持ち、feed()で少しずつパースを進める
class HttpRequest {
 private:
  // 生データ管理
  std::string _buffer;
  ParseState _parseState;
  ErrorCode _error;

  // ヘッダーパース用カウンタ（DoS対策）
  size_t _headerCount;
  size_t _totalHeaderSize;

  // パース結果
  HttpMethod _method;
  std::string _path;
  std::string _query;
  std::string _version;  // HTTP/1.1
  std::map<std::string, std::string> _headers;
  std::vector<char> _body;
  size_t _contentLength;  // Content-Lengthヘッダーの値
  bool _isChunked;        // Transfer-Encoding: chunked かどうか

  // chunkedパース用状態
  enum ChunkState {
    CHUNK_SIZE_LINE,  // チャンクサイズ行を読み取り中
    CHUNK_DATA,       // チャンクデータを読み取り中
    CHUNK_DATA_CRLF,  // データ後の \r\n を待機中
    CHUNK_FINAL_CRLF  // 終端チャンク後の \r\n (またはtrailer)
  };
  ChunkState _chunkState;
  size_t _currentChunkSize;  // 現在のチャンクサイズ
  size_t _chunkBytesRead;    // 現在のチャンクで読み取ったバイト数
  size_t _trailerCount;      // trailer行数カウンタ（DoS対策）

  // 紐付いた設定（パース完了後にセットされる）
  const ServerConfig* _config;
  const LocationConfig* _location;

  // 内部ヘルパー
  void parseRequestLine();
  void parseHeaders();
  void parseBody();               // ボディ解析のディスパッチャ
  void parseBodyContentLength();  // Content-Length ベースのボディ解析
  void parseBodyChunked();        // chunked ベースのボディ解析
  bool parseChunkSizeLine();      // チャンクサイズ行をパース
  bool parseChunkData();          // チャンクデータを読み取り
  bool parseChunkDataCRLF();      // データ後の \r\n を消費
  bool parseChunkFinalCRLF();     // 終端の \r\n を消費
  void setError(ErrorCode err);   // エラー状態をセットしREQ_ERRORに遷移
  size_t getMaxBodySize() const;  // client_max_body_size を取得

 public:
  HttpRequest();
  ~HttpRequest();

  // データを追加しパースを実行。完了したら true を返す。
  bool feed(const char* data, size_t size);

  // 状態確認
  bool isComplete() const;
  bool hasError() const;

  // Keep-Alive用にリセット
  void clear();

  // Getter / Setter
  HttpMethod getMethod() const;
  std::string getPath() const;
  std::string getHeader(const std::string& key) const;
  const std::vector<char>& getBody() const;
  size_t getContentLength() const;
  std::string HttpRequest::getQuery() const;
  void setConfig(const ServerConfig* config);
  const ServerConfig* getConfig() const;

  void setPath(const std::string& path);
};

// --- HTTP Response ---
// ステータスコード等からレスポンスを生データ列に変換する
class HttpResponse {
 private:
  enum ResState {
    RES_HEADER,
    RES_BODY,
    RES_FINISH,
    RES_DONE,
    RES_ERROR,
  } _state;

  int _statusCode;
  std::string _statusMessage;
  std::map<std::string, std::string> _headers;
  std::vector<char> _body;
  std::ifstream* _bodyFileStream;
  std::vector<char> _readBuffer;
  HttpMethod _requestMethod;
  std::string _errorMessage;

  // Chunked Transfer Encoding関連
  bool _isChunked;
  size_t _chunkSize;

  // 送信バッファ管理
  std::vector<char> _responseBuffer;  // ヘッダ+ボディの完成形
  size_t _sentBytes;                  // 送信済みバイト数

 public:
  HttpResponse();
  ~HttpResponse();
  HttpResponse(const HttpResponse& other);
  HttpResponse& operator=(const HttpResponse& other);
  void clear();

  // レスポンス構築用メソッド
  void setStatusCode(int code);
  void setHeader(const std::string& key, const std::string& value);
  void setBody(const std::string& body);
  void setBody(const std::vector<char>& body);
  bool setBodyFile(
      const std::string& filepath);  // ファイルを読み込んでBodyにする
  void setChunked(bool isChunked);
  // 将来HEADに対応する場合に必要になるので一応
  void setRequestMethod(HttpMethod method);

  // ErrorPage生成用
  void makeErrorResponse(int code, const ServerConfig* config = NULL);

  // 送信準備: ヘッダとボディを結合して _responseBuffer を作る
  void build();

  // epollループで使う送信メソッド
  const char* getData() const;
  size_t getRemainingSize() const;
  void advance(size_t n);  // nバイト送信完了
  bool isDone() const;
  bool isError() const;
  std::string getErrorMessage() const;

  // ヘルパー関数
  static std::string getMimeType(const std::string& filepath);
  static std::string buildErrorHtml(int code, const std::string& message);
  static bool isBodyForbidden(int code);

  //debug用: テスト時のみ有効化
#ifdef ENABLE_TEST_FRIENDS
  friend void inspectBuffer(const HttpResponse& res);
  friend void inspectResponse(const HttpResponse& res,
                              const std::string& checkName, size_t expSize,
                              const std::string& expType);
  friend void inspectResponse(const HttpResponse& res,
                              const std::string& checkName, const char* expData,
                              size_t expSize, const std::string& expType);
  friend void inspectErrorResponse(const HttpResponse& res, int expCode,
                                   const std::string& expMsg);
  friend void inspectClear(const HttpResponse& res);
  friend void inspectReuse(const HttpResponse& res, int expCode);
  friend std::string getRawBuffer(const HttpResponse& res);
  friend void inspectChunkedResponse(const HttpResponse& res,
                                     const std::string& testName,
                                     size_t originalBodySize);
#endif
};

#endif
