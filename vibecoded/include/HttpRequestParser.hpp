#ifndef HTTP_REQUEST_PARSER_HPP
#define HTTP_REQUEST_PARSER_HPP

#include "HttpRequest.hpp"
#include <string>
#include <cstddef>

/**
 * HttpRequestParser クラス
 * 
 * 状態機械パターンを使用して、HTTPリクエストを段階的にパースする。
 * 非同期I/Oに対応しており、データが到着した分だけパースを進める。
 * 
 * 状態遷移:
 *   PARSE_REQUEST_LINE -> PARSE_HEADERS -> PARSE_BODY -> PARSE_DONE
 *                                        -> PARSE_CHUNK_SIZE -> PARSE_CHUNK_BODY -> ...
 *   どの段階でもエラーが発生した場合は PARSE_ERROR に遷移
 */
class HttpRequestParser {
public:
    /**
     * パーサーの状態
     */
    enum ParseState {
        PARSE_REQUEST_LINE,    // リクエストライン("GET / HTTP/1.1")をパース中
        PARSE_HEADERS,         // ヘッダーフィールドをパース中
        PARSE_BODY,            // ボディ(Content-Lengthベース)をパース中
        PARSE_CHUNK_SIZE,      // チャンク転送のサイズ行をパース中
        PARSE_CHUNK_DATA,      // チャンクデータをパース中
        PARSE_CHUNK_TRAILER,   // チャンク末尾の改行をパース中
        PARSE_DONE,            // パース完了
        PARSE_ERROR            // パースエラー
    };
    
    /**
     * コンストラクタ
     * @param maxBodySize ボディの最大サイズ(バイト単位)
     */
    HttpRequestParser(size_t maxBodySize = 1048576); // デフォルト1MB
    
    /**
     * デストラクタ
     */
    ~HttpRequestParser();
    
    /**
     * データを追加してパースを進める
     * 
     * @param data 追加するデータ
     * @param len データの長さ
     * @return true=パース継続中またはPARSE_DONE, false=PARSE_ERROR
     */
    bool parse(const char* data, size_t len);
    
    /**
     * パース状態を取得
     */
    ParseState getState() const;
    
    /**
     * パース完了したHttpRequestを取得
     * @return HttpRequestオブジェクト(PARSE_DONEの場合のみ有効)
     */
    const HttpRequest& getRequest() const;
    
    /**
     * エラーメッセージを取得
     */
    const std::string& getErrorMessage() const;
    
    /**
     * パーサーをリセット(再利用可能にする)
     */
    void reset();

private:
    // 状態とデータ
    ParseState _state;
    HttpRequest _request;
    std::string _buffer;         // 未処理データを保持するバッファ
    std::string _errorMessage;
    size_t _maxBodySize;
    
    // チャンク転送用
    size_t _currentChunkSize;    // 現在処理中のチャンクサイズ
    size_t _currentChunkRead;    // 現在処理中のチャンクから読み込んだバイト数
    
    // 内部パース関数
    bool parseRequestLine();
    bool parseHeaders();
    bool parseBody();
    bool parseChunkSize();
    bool parseChunkData();
    bool parseChunkTrailer();
    
    // ヘルパー関数
    void processUri();           // URIからpath/queryを分離
    void processHost();          // HostヘッダーからホストとポートJを抽出
    std::string toLowerCase(const std::string& str);
    bool isValidMethod(const std::string& method);
    
    // エラー処理
    void setError(const std::string& message);
};

#endif // HTTP_REQUEST_PARSER_HPP
