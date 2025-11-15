#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>

/**
 * HttpResponse クラス
 * 
 * HTTPレスポンスを表現し、シリアライズする。
 * - ステータスライン(HTTPバージョン、ステータスコード、理由フレーズ)
 * - ヘッダーフィールド
 * - ボディデータ
 */
class HttpResponse {
public:
    /**
     * コンストラクタ
     */
    HttpResponse();
    
    /**
     * デストラクタ
     */
    ~HttpResponse();
    
    /**
     * ステータスコードを設定
     */
    void setStatusCode(int code);
    
    /**
     * ステータスコードを取得
     */
    int getStatusCode() const;
    
    /**
     * 理由フレーズを設定
     */
    void setReasonPhrase(const std::string& phrase);
    
    /**
     * 理由フレーズを取得
     */
    const std::string& getReasonPhrase() const;
    
    /**
     * ヘッダーを設定
     */
    void setHeader(const std::string& key, const std::string& value);
    
    /**
     * ボディを設定
     */
    void setBody(const std::string& body);
    
    /**
     * ボディを追加
     */
    void appendBody(const std::string& data);
    
    /**
     * ボディを取得
     */
    const std::string& getBody() const;
    
    /**
     * HTTPレスポンスを文字列にシリアライズ
     * @return "HTTP/1.1 200 OK\r\nContent-Type: ...\r\n\r\n..." 形式の文字列
     */
    std::string serialize() const;
    
    /**
     * ステータスコードから標準的な理由フレーズを取得
     */
    static std::string getDefaultReasonPhrase(int statusCode);

private:
    int _statusCode;
    std::string _reasonPhrase;
    std::map<std::string, std::string> _headers;
    std::string _body;
};

#endif // HTTP_RESPONSE_HPP
