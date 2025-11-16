#ifndef STATIC_FILE_HANDLER_HPP
#define STATIC_FILE_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Config.hpp"
#include <string>
#include <vector>

/**
 * StaticFileHandler クラス
 * 
 * 静的ファイルのGETリクエストを処理する。
 * - ファイルの読み込み
 * - ディレクトリのインデックスファイル処理
 * - autoindex（ディレクトリリスト）の生成
 */
class StaticFileHandler {
public:
    /**
     * GETリクエストを処理してレスポンスを生成
     * 
     * @param request HTTPリクエスト
     * @param serverConfig サーバー設定
     * @param locationConfig ロケーション設定（NULLの場合はサーバーのデフォルト設定を使用）
     * @return HTTPレスポンス
     */
    static HttpResponse handleGet(
        const HttpRequest& request,
        const ServerConfig& serverConfig,
        const LocationConfig* locationConfig
    );

    /**
     * DELETEリクエストを処理してレスポンスを生成
     * 
     * @param request HTTPリクエスト
     * @param serverConfig サーバー設定
     * @param locationConfig ロケーション設定
     * @return HTTPレスポンス
     */
    static HttpResponse handleDelete(
        const HttpRequest& request,
        const ServerConfig& serverConfig,
        const LocationConfig* locationConfig
    );

private:
    /**
     * 実ファイルパスを構築
     * @param locationConfig ロケーション設定
     * @param requestPath リクエストパス
     * @return 実ファイルパス
     */
    static std::string buildFilePath(
        const LocationConfig* locationConfig,
        const std::string& requestPath
    );
    
    /**
     * パスがディレクトリかどうかをチェック
     */
    static bool isDirectory(const std::string& path);
    
    /**
     * ファイルが存在するかチェック
     */
    static bool fileExists(const std::string& path);
    
    /**
     * ファイルを読み込む
     * @param path ファイルパス
     * @param content 読み込んだ内容を格納する変数
     * @return 成功したらtrue
     */
    static bool readFile(const std::string& path, std::string& content);
    
    /**
     * ディレクトリリスト（autoindex）のHTMLを生成
     * @param dirPath ディレクトリパス
     * @param requestPath リクエストパス（表示用）
     * @return HTMLコンテンツ
     */
    static std::string generateDirectoryListing(
        const std::string& dirPath,
        const std::string& requestPath
    );
    
    /**
     * ディレクトリ内のファイル一覧を取得
     * @param dirPath ディレクトリパス
     * @return ファイル名のリスト
     */
    static std::vector<std::string> listDirectory(const std::string& dirPath);
};

#endif // STATIC_FILE_HANDLER_HPP
