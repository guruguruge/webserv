#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include <vector>

/**
 * HttpRequest構造体
 * 
 * パース済みのHTTPリクエストを表現する。
 * - リクエストライン(メソッド、URI、HTTPバージョン)
 * - ヘッダーフィールド
 * - ボディデータ
 * - その他のメタデータ(Hostヘッダーから抽出した情報など)
 */
struct HttpRequest {
    // リクエストライン
    std::string method;          // "GET", "POST", "DELETE" など
    std::string uri;             // "/index.html", "/upload" など
    std::string httpVersion;     // "HTTP/1.1" など
    
    // ヘッダーフィールド (小文字化されたキー -> 値)
    std::map<std::string, std::string> headers;
    
    // ボディデータ
    std::vector<char> body;
    
    // Hostヘッダーから抽出した情報
    std::string host;            // "localhost" など
    std::string port;            // "8080" など (省略時は空文字列)
    
    // クエリパラメータ (URIから分離)
    std::string path;            // "/index.html" (クエリ文字列を除いたパス)
    std::string query;           // "key=value&foo=bar" (クエリ文字列部分)
    
    // コンストラクタ
    HttpRequest();
};

#endif // HTTP_REQUEST_HPP
